// SPDX-License-Identifier: GPL-3.0-or-later
#include "CPU_P32_6_Cycle.h"
#include "DMA.h"
#include "spdlog/spdlog.h"
#include <iostream>

namespace riscv_tlm {

CPURV32P6_Cycle::CPURV32P6_Cycle(sc_core::sc_module_name const& name,
                                 BaseType PC,
                                 bool debug)
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

    pc_register = PC;

    SC_THREAD(cycle_thread);

    logger->info("Created CPURV32P6_Cycle (6-stage cycle-accurate) CPU");
}

CPURV32P6_Cycle::~CPURV32P6_Cycle() {
    delete register_bank;
    delete mem_intf;
    delete base_inst;
    delete c_inst;
    delete m_inst;
    delete a_inst;
    // Note: m_qk is allocated by base CPU class but not used by cycle-accurate model
    // Base class destructor should handle cleanup if needed
}

void CPURV32P6_Cycle::set_clock(sc_core::sc_clock* c) {
    clk = c;
    if (clk) clock_period = clk->period();
}

void CPURV32P6_Cycle::cycle_thread() {
    std::cout << "[DEBUG] Paying cycle_thread started" << std::endl;
    while (true) {
        // Increment cycle count
        stats.cycles++;
        if (clk) {
             sc_core::wait(clk->posedge_event());
        }
        else {
             sc_core::wait(clock_period);
             std::cout << "[DEBUG] Wait period" << std::endl;
        }

        // Transfer latches
        mem_wb_reg = mem_wb_next;
        ex_mem_reg = ex_mem_next;
        is_ex_reg = is_ex_next;
        id_is_reg = id_is_next;
        if_id_reg = if_id_next;

        // Execute stages in reverse order
        // std::cout << "[DEBUG] WB" << std::endl;
        WB_stage();
        // std::cout << "[DEBUG] MEM" << std::endl;
        MEM_stage();
        // std::cout << "[DEBUG] EX" << std::endl;
        EX_stage();
        // std::cout << "[DEBUG] IS" << std::endl;
        IS_stage();
        // std::cout << "[DEBUG] ID" << std::endl;
        ID_stage();
        // std::cout << "[DEBUG] IF" << std::endl;
        IF_stage();
    }
}

void CPURV32P6_Cycle::pc_select() {
    if (pc_redirect_valid) {
        pc_register = pc_redirect_target;
        pc_redirect_valid = false;
        return;
    }
    if (stall_fetch) return;
    pc_register += 4;
}

void CPURV32P6_Cycle::IF_stage() {
    // Stall if a DMA transfer is still in flight
    while (riscv_tlm::peripherals::DMA::is_in_flight()) {
        if (clk) wait(clk->posedge_event());
        else wait(clock_period);
    }
    pc_select();
    if (flush_pipeline) {
        if_id_next.valid = false;
        return;
    }
    uint32_t instr = 0;
    if (fetch_instruction(pc_register, instr)) {
        if_id_next.pc = pc_register;
        if_id_next.instr = instr;
        if_id_next.valid = true;
    } else {
        if_id_next.valid = false;
    }
}

bool CPURV32P6_Cycle::fetch_instruction(uint32_t addr, uint32_t& data) {
    if (dmi_ptr_valid && addr >= dmi_start_addr && (addr + 4) <= dmi_end_addr) {
        std::memcpy(&data, dmi_ptr + (addr - dmi_start_addr), 4);
        return true;
    }

    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    instr_bus->b_transport(trans, delay);

    if (trans.is_response_error()) return false;

    if (trans.is_dmi_allowed()) {
        tlm::tlm_dmi dmi_data;
        if (instr_bus->get_direct_mem_ptr(trans, dmi_data)) {
            dmi_ptr_valid = true;
            dmi_ptr = dmi_data.get_dmi_ptr();
            dmi_start_addr = dmi_data.get_start_address();
            dmi_end_addr = dmi_data.get_end_address();
        }
    }
    return true;
}

void CPURV32P6_Cycle::ID_stage() {
    if (!if_id_reg.valid) {
        id_is_next.valid = false;
        return;
    }

    uint32_t instr = if_id_reg.instr;
    
    id_is_next.pc = if_id_reg.pc;
    id_is_next.instr = instr;
    id_is_next.opcode = instr & 0x7F;
    id_is_next.rd = (instr >> 7) & 0x1F;
    id_is_next.funct3 = (instr >> 12) & 0x7;
    id_is_next.rs1 = (instr >> 15) & 0x1F;
    id_is_next.rs2 = (instr >> 20) & 0x1F;
    id_is_next.funct7 = (instr >> 25) & 0x7F;

    switch (id_is_next.opcode) {
        case 0x13: case 0x03: case 0x67:
            id_is_next.imm = static_cast<int32_t>(instr) >> 20;
            break;
        case 0x23:
            id_is_next.imm = static_cast<int32_t>(((instr >> 25) << 5) | ((instr >> 7) & 0x1F)) << 20 >> 20;
            break;
        case 0x63:
            id_is_next.imm = static_cast<int32_t>(
                ((instr >> 31) << 12) | (((instr >> 7) & 1) << 11) |
                (((instr >> 25) & 0x3F) << 5) | (((instr >> 8) & 0xF) << 1)) << 19 >> 19;
            break;
        case 0x37: case 0x17:
            id_is_next.imm = static_cast<int32_t>(instr & 0xFFFFF000);
            break;
        case 0x6F:
            id_is_next.imm = static_cast<int32_t>(
                ((instr >> 31) << 20) | (((instr >> 12) & 0xFF) << 12) |
                (((instr >> 20) & 1) << 11) | (((instr >> 21) & 0x3FF) << 1)) << 11 >> 11;
            break;
        default:
            id_is_next.imm = 0;
    }
    id_is_next.valid = true;
}

void CPURV32P6_Cycle::IS_stage() {
    if (!id_is_reg.valid) {
        is_ex_next.valid = false;
        return;
    }
    if (scoreboard[id_is_reg.rs1] || scoreboard[id_is_reg.rs2]) return;

    is_ex_next.pc = id_is_reg.pc;
    is_ex_next.rs1_val = register_bank->getValue(id_is_reg.rs1);
    is_ex_next.rs2_val = register_bank->getValue(id_is_reg.rs2);
    is_ex_next.imm = id_is_reg.imm;
    is_ex_next.rd = id_is_reg.rd;
    is_ex_next.opcode = id_is_reg.opcode;
    is_ex_next.funct3 = id_is_reg.funct3;
    is_ex_next.funct7 = id_is_reg.funct7;
    is_ex_next.valid = true;

    if (id_is_reg.rd != 0) scoreboard[id_is_reg.rd] = true;
}

void CPURV32P6_Cycle::EX_stage() {
    if (!is_ex_reg.valid) {
        ex_mem_next.valid = false;
        return;
    }

    uint32_t result = 0;
    bool branch_taken = false;
    uint32_t branch_target = 0;
    bool mem_read = false;
    bool mem_write = false;

    switch (is_ex_reg.opcode) {
        case 0x33: // R-type
            switch (is_ex_reg.funct3) {
                case 0x0:
                    if (is_ex_reg.funct7 == 0x20) result = is_ex_reg.rs1_val - is_ex_reg.rs2_val;
                    else result = is_ex_reg.rs1_val + is_ex_reg.rs2_val;
                    break;
                case 0x1: result = is_ex_reg.rs1_val << (is_ex_reg.rs2_val & 0x1F); break;
                case 0x2: result = (static_cast<int32_t>(is_ex_reg.rs1_val) < static_cast<int32_t>(is_ex_reg.rs2_val)); break;
                case 0x3: result = (is_ex_reg.rs1_val < is_ex_reg.rs2_val); break;
                case 0x4: result = is_ex_reg.rs1_val ^ is_ex_reg.rs2_val; break;
                case 0x5:
                    if (is_ex_reg.funct7 == 0x20) result = static_cast<int32_t>(is_ex_reg.rs1_val) >> (is_ex_reg.rs2_val & 0x1F);
                    else result = is_ex_reg.rs1_val >> (is_ex_reg.rs2_val & 0x1F);
                    break;
                case 0x6: result = is_ex_reg.rs1_val | is_ex_reg.rs2_val; break;
                case 0x7: result = is_ex_reg.rs1_val & is_ex_reg.rs2_val; break;
            }
            break;

        case 0x13: // I-type ALU
            switch (is_ex_reg.funct3) {
                case 0x0: result = is_ex_reg.rs1_val + is_ex_reg.imm; break;
                case 0x2: result = (static_cast<int32_t>(is_ex_reg.rs1_val) < is_ex_reg.imm); break;
                case 0x3: result = (is_ex_reg.rs1_val < static_cast<uint32_t>(is_ex_reg.imm)); break;
                case 0x4: result = is_ex_reg.rs1_val ^ is_ex_reg.imm; break;
                case 0x6: result = is_ex_reg.rs1_val | is_ex_reg.imm; break;
                case 0x7: result = is_ex_reg.rs1_val & is_ex_reg.imm; break;
                case 0x1: result = is_ex_reg.rs1_val << (is_ex_reg.imm & 0x1F); break;
                case 0x5:
                    if ((is_ex_reg.imm & 0x400) != 0) result = static_cast<int32_t>(is_ex_reg.rs1_val) >> (is_ex_reg.imm & 0x1F);
                    else result = is_ex_reg.rs1_val >> (is_ex_reg.imm & 0x1F);
                    break;
            }
            break;

        case 0x03: result = is_ex_reg.rs1_val + is_ex_reg.imm; mem_read = true; break;
        case 0x23: result = is_ex_reg.rs1_val + is_ex_reg.imm; mem_write = true; break;

        case 0x63: // Branch
            branch_target = is_ex_reg.pc + is_ex_reg.imm;
            switch (is_ex_reg.funct3) {
                case 0x0: branch_taken = (is_ex_reg.rs1_val == is_ex_reg.rs2_val); break;
                case 0x1: branch_taken = (is_ex_reg.rs1_val != is_ex_reg.rs2_val); break;
                case 0x4: branch_taken = (static_cast<int32_t>(is_ex_reg.rs1_val) < static_cast<int32_t>(is_ex_reg.rs2_val)); break;
                case 0x5: branch_taken = (static_cast<int32_t>(is_ex_reg.rs1_val) >= static_cast<int32_t>(is_ex_reg.rs2_val)); break;
                case 0x6: branch_taken = (is_ex_reg.rs1_val < is_ex_reg.rs2_val); break;
                case 0x7: branch_taken = (is_ex_reg.rs1_val >= is_ex_reg.rs2_val); break;
            }
            if (branch_taken) {
                pc_redirect_target = branch_target;
                pc_redirect_valid = true;
                flush_pipeline = true;
            }
            break;

        case 0x6F: result = is_ex_reg.pc + 4; pc_redirect_target = is_ex_reg.pc + is_ex_reg.imm; pc_redirect_valid = true; flush_pipeline = true; break;
        case 0x67: result = is_ex_reg.pc + 4; pc_redirect_target = (is_ex_reg.rs1_val + is_ex_reg.imm) & ~1; pc_redirect_valid = true; flush_pipeline = true; break;
        case 0x37: result = is_ex_reg.imm; break;
        case 0x17: result = is_ex_reg.pc + is_ex_reg.imm; break;
    }

    ex_mem_next.pc = is_ex_reg.pc;
    ex_mem_next.alu_result = result;
    ex_mem_next.store_data = is_ex_reg.rs2_val;
    ex_mem_next.rd = is_ex_reg.rd;
    ex_mem_next.funct3 = is_ex_reg.funct3;
    ex_mem_next.mem_read = mem_read;
    ex_mem_next.mem_write = mem_write;
    ex_mem_next.branch_taken = branch_taken;
    ex_mem_next.branch_target = branch_target;
    ex_mem_next.valid = true;
}

void CPURV32P6_Cycle::MEM_stage() {
    if (!ex_mem_reg.valid) {
        mem_wb_next.valid = false;
        return;
    }

    uint32_t result = ex_mem_reg.alu_result;

    if (ex_mem_reg.mem_read) {
        switch (ex_mem_reg.funct3) {
            case 0x0: result = static_cast<int32_t>(static_cast<int8_t>(mem_intf->readDataMem(ex_mem_reg.alu_result, 1))); break;
            case 0x1: result = static_cast<int32_t>(static_cast<int16_t>(mem_intf->readDataMem(ex_mem_reg.alu_result, 2))); break;
            case 0x2: result = mem_intf->readDataMem(ex_mem_reg.alu_result, 4); break;
            case 0x4: result = mem_intf->readDataMem(ex_mem_reg.alu_result, 1); break;
            case 0x5: result = mem_intf->readDataMem(ex_mem_reg.alu_result, 2); break;
        }
    } else if (ex_mem_reg.mem_write) {
        switch (ex_mem_reg.funct3) {
            case 0x0: mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 1); break;
            case 0x1: mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 2); break;
            case 0x2: mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 4); break;
        }
    }

    mem_wb_next.result = result;
    mem_wb_next.rd = ex_mem_reg.rd;
    mem_wb_next.reg_write = (ex_mem_reg.rd != 0) && !ex_mem_reg.mem_write;
    mem_wb_next.valid = true;
}

void CPURV32P6_Cycle::WB_stage() {
    if (!mem_wb_reg.valid) return;
    if (mem_wb_reg.reg_write && mem_wb_reg.rd != 0) {
        register_bank->setValue(mem_wb_reg.rd, mem_wb_reg.result);
        scoreboard[mem_wb_reg.rd] = false;
    }
    // Increment retired instruction count for cycle‑accurate stats
    stats.instructions++;
    perf->instructionsInc();
}

bool CPURV32P6_Cycle::cpu_process_IRQ() { return false; }

void CPURV32P6_Cycle::call_interrupt(tlm::tlm_generic_payload& m_trans, sc_core::sc_time& delay) {
    interrupt = true;
    memcpy(&int_cause, m_trans.get_data_ptr(), sizeof(BaseType));
    delay = sc_core::SC_ZERO_TIME;
}

std::uint64_t CPURV32P6_Cycle::getStartDumpAddress() {
    return register_bank->getValue(Registers<BaseType>::t0);
}

std::uint64_t CPURV32P6_Cycle::getEndDumpAddress() {
    return register_bank->getValue(Registers<BaseType>::t1);
}

void CPURV32P6_Cycle::printStats() const {
    std::cout << "  Architecture: RV32\n";
    std::cout << "  Cycles:       " << stats.cycles << "\n";
    std::cout << "  Instructions: " << stats.instructions << "\n";
    std::cout << "  CPI:          " << std::fixed << std::setprecision(2) << stats.get_cpi() << "\n";
}

} // namespace riscv_tlm
