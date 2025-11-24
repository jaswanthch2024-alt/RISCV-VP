// SPDX-License-Identifier: GPL-3.0-or-later
#include "CPU_P32.h"
#include "spdlog/spdlog.h"

namespace riscv_tlm {

CPURV32P::CPURV32P(sc_core::sc_module_name const& name, BaseType PC, bool debug)
    : CPU(name, debug) {

    register_bank = new Registers<BaseType>();
    mem_intf = new MemoryInterface();

    register_bank->setPC(PC);
    register_bank->setValue(Registers<BaseType>::sp, (Memory::SIZE / 4) - 1);
    int_cause = 0;

    instr_bus.register_invalidate_direct_mem_ptr(this, &CPU::invalidate_direct_mem_ptr);

    base_inst = new BASE_ISA<BaseType>(0, register_bank, mem_intf);
    c_inst    = new C_extension<BaseType>(0, register_bank, mem_intf);
    m_inst    = new M_extension<BaseType>(0, register_bank, mem_intf);
    a_inst    = new A_extension<BaseType>(0, register_bank, mem_intf);

    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&if_id_next.instr));

    logger->info("Created CPURV32P (pipelined) CPU");
    std::cout << "Created CPURV32P (pipelined) CPU" << std::endl;
}

CPURV32P::~CPURV32P() {
    delete register_bank;
    delete mem_intf;
    delete base_inst;
    delete c_inst;
    delete m_inst;
    delete a_inst;
    delete m_qk;
}

bool CPURV32P::CPU_step() {
    bool breakpoint = false;
    // One pipeline cycle: advance all stages then latch IF/ID.
    stageWB(breakpoint);
    stageMEM();
    stageEX();
    stageID();
    stageIF();
    if_id = if_id_next;
    // Timing: handled in CPU::CPU_thread (single 10ns wait for pipelined cores).
    return breakpoint;
}

void CPURV32P::stageIF() {
    // Fetch 4 bytes at current architectural PC
    if_id_next = {};
    if_id_next.pc = register_bank->getPC();

    if (dmi_ptr_valid) {
        std::memcpy(&if_id_next.instr, dmi_ptr + if_id_next.pc, 4);
        // Optionally honor DMI latency by advancing local time
        if (m_qk) { m_qk->inc(sc_core::SC_ZERO_TIME); }
    } else {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        tlm::tlm_dmi dmi_data;
        trans.set_address(if_id_next.pc);
        instr_bus->b_transport(trans, delay);

        if (trans.is_response_error()) {
            SC_REPORT_ERROR("CPURV32P", "Instruction fetch error");
        }
        if (trans.is_dmi_allowed()) {
            dmi_ptr_valid = instr_bus->get_direct_mem_ptr(trans, dmi_data);
            if (dmi_ptr_valid) {
                dmi_ptr = dmi_data.get_dmi_ptr();
            }
        }
        if (m_qk) {
            m_qk->inc(delay);
            if (m_qk->need_sync()) m_qk->sync();
        }
    }

    if_id_next.valid = true;
    Performance::getInstance()->codeMemoryRead();
}

void CPURV32P::stageWB(bool &breakpoint) {
    if (!if_id.valid) {
        breakpoint = false;
        return;
    }

    // Execute the fetched instruction at the snapshot PC
    Instruction inst(if_id.instr);
    breakpoint = false;

    // Align PC to the fetched value before executing
    register_bank->setPC(if_id.pc);

    base_inst->setInstr(if_id.instr);
    auto deco = base_inst->decode();

    if (deco != OP_ERROR) {
        auto PC_not_affected = base_inst->exec_instruction(inst, &breakpoint, deco);
        if (PC_not_affected) {
            register_bank->incPC();
        }
    } else {
        c_inst->setInstr(if_id.instr);
        auto c_deco = c_inst->decode();
        if (c_deco != OP_C_ERROR) {
            auto PC_not_affected = c_inst->exec_instruction(inst, &breakpoint, c_deco);
            if (PC_not_affected) {
                register_bank->incPCby2();
            }
        } else {
            m_inst->setInstr(if_id.instr);
            auto m_deco = m_inst->decode();
            if (m_deco != OP_M_ERROR) {
                auto PC_not_affected = m_inst->exec_instruction(inst, m_deco);
                if (PC_not_affected) {
                    register_bank->incPC();
                }
            } else {
                a_inst->setInstr(if_id.instr);
                auto a_deco = a_inst->decode();
                if (a_deco != OP_A_ERROR) {
                    auto PC_not_affected = a_inst->exec_instruction(inst, a_deco);
                    if (PC_not_affected) {
                        register_bank->incPC();
                    }
                } else {
                    std::cout << "Extension not implemented yet" << std::endl;
                    inst.dump();
                    base_inst->NOP();
                    register_bank->incPC();
                }
            }
        }
    }

    if (breakpoint) {
        std::cout << "Breakpoint set to true\n";
    }

    Performance::getInstance()->instructionsInc();

    // Consume WB entry
    if_id.valid = false;
}

bool CPURV32P::cpu_process_IRQ() {
    BaseType csr_temp;
    bool ret_value = false;

    if (interrupt) {
        csr_temp = register_bank->getCSR(CSR_MSTATUS);
        if ((csr_temp & MSTATUS_MIE) == 0) {
            return ret_value;
        }

        csr_temp = register_bank->getCSR(CSR_MIP);
        if ((csr_temp & MIP_MEIP) == 0) {
            csr_temp |= MIP_MEIP;
            register_bank->setCSR(CSR_MIP, csr_temp);

            BaseType old_pc = register_bank->getPC();
            register_bank->setCSR(CSR_MEPC, old_pc);
            register_bank->setCSR(CSR_MCAUSE, 0x80000000);
            BaseType new_pc = register_bank->getCSR(CSR_MTVEC);
            register_bank->setPC(new_pc);

            ret_value = true;
            interrupt = false;
            irq_already_down = false;
        }
    } else {
        if (!irq_already_down) {
            csr_temp = register_bank->getCSR(CSR_MIP);
            csr_temp &= ~MIP_MEIP;
            register_bank->setCSR(CSR_MIP, csr_temp);
            irq_already_down = true;
        }
    }
    return ret_value;
}

void CPURV32P::call_interrupt(tlm::tlm_generic_payload &m_trans, sc_core::sc_time &delay) {
    interrupt = true;
    memcpy(&int_cause, m_trans.get_data_ptr(), sizeof(BaseType));
    delay = sc_core::SC_ZERO_TIME;
}

std::uint64_t CPURV32P::getStartDumpAddress() {
    return register_bank->getValue(Registers<std::uint32_t>::t0);
}

std::uint64_t CPURV32P::getEndDumpAddress() {
    return register_bank->getValue(Registers<std::uint32_t>::t1);
}

} // namespace riscv_tlm
