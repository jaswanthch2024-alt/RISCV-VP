// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file CPU_P32_2.cpp
 * @brief 2-Stage Pipelined RISC-V 32-bit CPU Implementation (AT model for VP)
 * 
 * Approximately-Timed (AT) 2-stage pipeline: IF -> EX
 * - IF: Fetch instruction
 * - EX: Decode + Execute + Memory + Writeback
 * 
 * This model waits for actual clock cycles to provide timing accuracy.
 * Branch taken causes 1-cycle flush penalty.
 */
#include "CPU_P32_2.h"
#include "spdlog/spdlog.h"

namespace riscv_tlm {

CPURV32P2::CPURV32P2(sc_core::sc_module_name const& name, BaseType PC, bool debug)
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

    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&INSTR));
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    logger->info("Created CPURV32P2 (2-stage AT pipelined) CPU for VP");
    std::cout << "Created CPURV32P2 (2-stage AT pipelined) CPU for VP" << std::endl;
}

CPURV32P2::~CPURV32P2() {
    delete register_bank;
    delete mem_intf;
    delete base_inst;
    delete c_inst;
    delete m_inst;
    delete a_inst;
    delete m_qk;
}

bool CPURV32P2::CPU_step() {
    bool breakpoint = false;
    
    stats.cycles++;

    // =========================================================================
    // Stage 1: Fetch instruction (IF)
    // =========================================================================
    if (dmi_ptr_valid) {
        std::memcpy(&INSTR, dmi_ptr + register_bank->getPC(), 4);
    } else {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        tlm::tlm_dmi dmi_data;
        trans.set_address(register_bank->getPC());
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        instr_bus->b_transport(trans, delay);

        if (trans.is_response_error()) {
            SC_REPORT_ERROR("CPURV32P2", "Instruction fetch error");
        }
        if (trans.is_dmi_allowed()) {
            dmi_ptr_valid = instr_bus->get_direct_mem_ptr(trans, dmi_data);
            if (dmi_ptr_valid) {
                dmi_ptr = dmi_data.get_dmi_ptr();
            }
        }
    }

    perf->codeMemoryRead();
    inst.setInstr(INSTR);

    // =========================================================================
    // Stage 2: Decode, Execute, Memory, Writeback (EX)
    // =========================================================================
    bool pc_changed = false;
    bool is_branch = false;

    // Try BASE ISA first
    base_inst->setInstr(INSTR);
    auto deco = base_inst->decode();

    if (deco != OP_ERROR) {
        std::uint32_t opcode = INSTR & 0x7F;
        is_branch = (opcode == 0x63 || opcode == 0x6F || opcode == 0x67);

        pc_changed = !base_inst->exec_instruction(inst, &breakpoint, deco);
        if (!pc_changed) {
            register_bank->incPC();
        }
    } else {
        // Try C extension
        c_inst->setInstr(INSTR);
        auto c_deco = c_inst->decode();

        if (c_deco != OP_C_ERROR) {
            is_branch = (c_deco == OP_C_J || c_deco == OP_C_JAL || 
                        c_deco == OP_C_JR || c_deco == OP_C_JALR ||
                        c_deco == OP_C_BEQZ || c_deco == OP_C_BNEZ);

            pc_changed = !c_inst->exec_instruction(inst, &breakpoint, c_deco);
            if (!pc_changed) {
                register_bank->incPCby2();
            }
        } else {
            // Try M extension
            m_inst->setInstr(INSTR);
            auto m_deco = m_inst->decode();

            if (m_deco != OP_M_ERROR) {
                pc_changed = !m_inst->exec_instruction(inst, m_deco);
                if (!pc_changed) {
                    register_bank->incPC();
                }
            } else {
                // Try A extension
                a_inst->setInstr(INSTR);
                auto a_deco = a_inst->decode();

                if (a_deco != OP_A_ERROR) {
                    pc_changed = !a_inst->exec_instruction(inst, a_deco);
                    if (!pc_changed) {
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

    // =========================================================================
    // AT Timing Model: Wait for clock cycles
    // =========================================================================
    // Base: 1 cycle per instruction
    sc_core::wait(clock_period);

    // Branch taken: +1 cycle penalty (flush IF stage)
    if (is_branch && pc_changed) {
        stats.cycles++;
        stats.flushes++;
        stats.control_hazards++;
        sc_core::wait(clock_period);  // Wait for flush penalty
    }

    perf->instructionsInc();

    return breakpoint;
}

// =============================================================================
// IRQ Processing
// =============================================================================
bool CPURV32P2::cpu_process_IRQ() {
    BaseType csr_temp;
    bool ret_value = false;

    if (interrupt) {
        csr_temp = register_bank->getCSR(CSR_MSTATUS);
        if ((csr_temp & MSTATUS_MIE) == 0) {
            logger->debug("{} ns. PC: 0x{:x}. Interrupt delayed", 
                         sc_core::sc_time_stamp().value(), register_bank->getPC());
            return ret_value;
        }

        csr_temp = register_bank->getCSR(CSR_MIP);
        if ((csr_temp & MIP_MEIP) == 0) {
            csr_temp |= MIP_MEIP;
            register_bank->setCSR(CSR_MIP, csr_temp);

            logger->debug("{} ns. PC: 0x{:x}. Interrupt!", 
                         sc_core::sc_time_stamp().value(), register_bank->getPC());

            BaseType old_pc = register_bank->getPC();
            register_bank->setCSR(CSR_MEPC, old_pc);
            register_bank->setCSR(CSR_MCAUSE, 0x80000000);
            BaseType new_pc = register_bank->getCSR(CSR_MTVEC);
            register_bank->setPC(new_pc);

            // Pipeline flush on interrupt (flush 1 stage + latency)
            stats.flushes++;
            stats.cycles += 2;
            sc_core::wait(clock_period * 2);  // AT: wait for IRQ latency

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

void CPURV32P2::call_interrupt(tlm::tlm_generic_payload &m_trans, sc_core::sc_time &delay) {
    interrupt = true;
    memcpy(&int_cause, m_trans.get_data_ptr(), sizeof(BaseType));
    delay = sc_core::SC_ZERO_TIME;
}

std::uint64_t CPURV32P2::getStartDumpAddress() {
    return register_bank->getValue(Registers<std::uint32_t>::t0);
}

std::uint64_t CPURV32P2::getEndDumpAddress() {
    return register_bank->getValue(Registers<std::uint32_t>::t1);
}

} // namespace riscv_tlm
