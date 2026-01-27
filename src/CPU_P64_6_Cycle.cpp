// SPDX-License-Identifier: GPL-3.0-or-later
#include "CPU_P64_6_Cycle.h"
#include "spdlog/spdlog.h"
#include <iostream>

namespace riscv_tlm {

CPURV64P6_Cycle::CPURV64P6_Cycle(sc_core::sc_module_name const& name,
                                 BaseType PC,
                                 bool debug)
    : CPU(name, debug) {

    register_bank = new Registers<BaseType>();
    mem_intf = new MemoryInterface();

    register_bank->setPC(PC);
    register_bank->setValue(Registers<BaseType>::sp, (Memory::SIZE / 8) - 1);
    int_cause = 0;

    instr_bus.register_invalidate_direct_mem_ptr(this, &CPU::invalidate_direct_mem_ptr);

    base_inst = new BASE_ISA<BaseType>(0, register_bank, mem_intf);
    c_inst    = new C_extension<BaseType>(0, register_bank, mem_intf);
    m_inst    = new M_extension<BaseType>(0, register_bank, mem_intf);
    a_inst    = new A_extension<BaseType>(0, register_bank, mem_intf);

    pc_register = PC;

    SC_THREAD(cycle_thread);

    logger->info("Created CPURV64P6_Cycle (6-stage cycle-accurate) CPU");
}

CPURV64P6_Cycle::~CPURV64P6_Cycle() {
    delete register_bank;
    delete mem_intf;
    delete base_inst;
    delete c_inst;
    delete m_inst;
    delete a_inst;
    // Note: m_qk is allocated by base CPU class but not used by cycle-accurate model
    // Base class destructor should handle cleanup if needed
}

void CPURV64P6_Cycle::set_clock(sc_core::sc_clock* c) {
    clk = c;
    if (clk) clock_period = clk->period();
}

// =============================================================================
// Main Cycle Thread
// =============================================================================

void CPURV64P6_Cycle::cycle_thread() {
    while (true) {
        if (clk) sc_core::wait(clk->posedge_event());
        else sc_core::wait(clock_period);

        // Transfer latches
        mem_wb_reg = mem_wb_next;
        ex_mem_reg = ex_mem_next;
        is_ex_reg = is_ex_next;
        id_is_reg = id_is_next;
        if_id_reg = if_id_next;

        // Execute stages in reverse order
        WB_stage();
        MEM_stage();
        EX_stage();
        IS_stage();
        ID_stage();
        IF_stage();
        
        // Update cycle count
        stats.cycles++;
        // Termination condition: stop if pipeline is empty and no more instructions
        if (!if_id_reg.valid && !id_is_reg.valid && !is_ex_reg.valid && !ex_mem_reg.valid && !mem_wb_reg.valid) {
            // No active instructions, exit simulation loop
            break;
        }
        // Safety guard: prevent infinite loops
        if (stats.cycles > 1000000) {
            std::cerr << "Simulation exceeded max cycles, terminating." << std::endl;
            break;
        }
    }
}

// =============================================================================
// PC Select
// =============================================================================

void CPURV64P6_Cycle::pc_select() {
    if (pc_redirect_valid) {
        pc_register = pc_redirect_target;
        pc_redirect_valid = false;
        return;
    }

    if (stall_fetch) {
        return;
    }

    pc_register += 4;
}


// =============================================================================
// IF Stage (Instruction Fetch)
// =============================================================================

void CPURV64P6_Cycle::IF_stage() {
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

bool CPURV64P6_Cycle::fetch_instruction(uint64_t addr, uint32_t& data) {
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

// =============================================================================
// ID Stage (Instruction Decode)
// =============================================================================

void CPURV64P6_Cycle::ID_stage() {
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

    // Decode immediate based on opcode
    switch (id_is_next.opcode) {
        case 0x13: // I-type (ADDI, etc.)
        case 0x03: // Load
        case 0x67: // JALR
            id_is_next.imm = static_cast<int64_t>(static_cast<int32_t>(instr) >> 20);
            break;
        case 0x23: // S-type (Store)
            id_is_next.imm = static_cast<int64_t>(
                static_cast<int32_t>(((instr >> 25) << 5) | ((instr >> 7) & 0x1F)) << 20 >> 20);
            break;
        case 0x63: // B-type (Branch)
            id_is_next.imm = static_cast<int64_t>(
                static_cast<int32_t>(
                    ((instr >> 31) << 12) | (((instr >> 7) & 1) << 11) |
                    (((instr >> 25) & 0x3F) << 5) | (((instr >> 8) & 0xF) << 1)) << 19 >> 19);
            break;
        case 0x37: // LUI
        case 0x17: // AUIPC
            id_is_next.imm = static_cast<int64_t>(static_cast<int32_t>(instr & 0xFFFFF000));
            break;
        case 0x6F: // JAL
            id_is_next.imm = static_cast<int64_t>(
                static_cast<int32_t>(
                    ((instr >> 31) << 20) | (((instr >> 12) & 0xFF) << 12) |
                    (((instr >> 20) & 1) << 11) | (((instr >> 21) & 0x3FF) << 1)) << 11 >> 11);
            break;
        default:
            id_is_next.imm = 0;
    }

    id_is_next.valid = true;
}

// =============================================================================
// IS Stage (Issue / Register Read)
// =============================================================================

void CPURV64P6_Cycle::IS_stage() {
    if (!id_is_reg.valid) {
        is_ex_next.valid = false;
        return;
    }

    // Check for hazards
    if (scoreboard[id_is_reg.rs1] || scoreboard[id_is_reg.rs2]) {
        // Stall - keep same values
        return;
    }

    // Read registers
    is_ex_next.pc = id_is_reg.pc;
    is_ex_next.rs1_val = register_bank->getValue(id_is_reg.rs1);
    is_ex_next.rs2_val = register_bank->getValue(id_is_reg.rs2);
    is_ex_next.imm = id_is_reg.imm;
    is_ex_next.rd = id_is_reg.rd;
    is_ex_next.opcode = id_is_reg.opcode;
    is_ex_next.funct3 = id_is_reg.funct3;
    is_ex_next.funct7 = id_is_reg.funct7;
    is_ex_next.valid = true;

    // Mark destination register as busy
    if (id_is_reg.rd != 0) {
        scoreboard[id_is_reg.rd] = true;
    }
}

// =============================================================================
// EX Stage (Execute)
// =============================================================================

void CPURV64P6_Cycle::EX_stage() {
    if (!is_ex_reg.valid) {
        ex_mem_next.valid = false;
        return;
    }

    uint64_t result = 0;
    bool branch_taken = false;
    uint64_t branch_target = 0;
    bool mem_read = false;
    bool mem_write = false;

    switch (is_ex_reg.opcode) {
        case 0x33: // R-type
            switch (is_ex_reg.funct3) {
                case 0x0: // ADD/SUB
                    if (is_ex_reg.funct7 == 0x20) result = is_ex_reg.rs1_val - is_ex_reg.rs2_val;
                    else result = is_ex_reg.rs1_val + is_ex_reg.rs2_val;
                    break;
                case 0x1: result = is_ex_reg.rs1_val << (is_ex_reg.rs2_val & 0x3F); break; // SLL
                case 0x2: result = (static_cast<int64_t>(is_ex_reg.rs1_val) < static_cast<int64_t>(is_ex_reg.rs2_val)); break; // SLT
                case 0x3: result = (is_ex_reg.rs1_val < is_ex_reg.rs2_val); break; // SLTU
                case 0x4: result = is_ex_reg.rs1_val ^ is_ex_reg.rs2_val; break; // XOR
                case 0x5: // SRL/SRA
                    if (is_ex_reg.funct7 == 0x20) result = static_cast<int64_t>(is_ex_reg.rs1_val) >> (is_ex_reg.rs2_val & 0x3F);
                    else result = is_ex_reg.rs1_val >> (is_ex_reg.rs2_val & 0x3F);
                    break;
                case 0x6: result = is_ex_reg.rs1_val | is_ex_reg.rs2_val; break; // OR
                case 0x7: result = is_ex_reg.rs1_val & is_ex_reg.rs2_val; break; // AND
            }
            break;

        case 0x13: // I-type ALU
            switch (is_ex_reg.funct3) {
                case 0x0: result = is_ex_reg.rs1_val + is_ex_reg.imm; break; // ADDI
                case 0x2: result = (static_cast<int64_t>(is_ex_reg.rs1_val) < is_ex_reg.imm); break; // SLTI
                case 0x3: result = (is_ex_reg.rs1_val < static_cast<uint64_t>(is_ex_reg.imm)); break; // SLTIU
                case 0x4: result = is_ex_reg.rs1_val ^ is_ex_reg.imm; break; // XORI
                case 0x6: result = is_ex_reg.rs1_val | is_ex_reg.imm; break; // ORI
                case 0x7: result = is_ex_reg.rs1_val & is_ex_reg.imm; break; // ANDI
                case 0x1: result = is_ex_reg.rs1_val << (is_ex_reg.imm & 0x3F); break; // SLLI
                case 0x5: // SRLI/SRAI
                    if ((is_ex_reg.imm & 0x400) != 0) result = static_cast<int64_t>(is_ex_reg.rs1_val) >> (is_ex_reg.imm & 0x3F);
                    else result = is_ex_reg.rs1_val >> (is_ex_reg.imm & 0x3F);
                    break;
            }
            break;

        case 0x03: // Load
            result = is_ex_reg.rs1_val + is_ex_reg.imm;
            mem_read = true;
            break;

        case 0x23: // Store
            result = is_ex_reg.rs1_val + is_ex_reg.imm;
            mem_write = true;
            break;

        case 0x63: // Branch
            branch_target = is_ex_reg.pc + is_ex_reg.imm;
            switch (is_ex_reg.funct3) {
                case 0x0: branch_taken = (is_ex_reg.rs1_val == is_ex_reg.rs2_val); break; // BEQ
                case 0x1: branch_taken = (is_ex_reg.rs1_val != is_ex_reg.rs2_val); break; // BNE
                case 0x4: branch_taken = (static_cast<int64_t>(is_ex_reg.rs1_val) < static_cast<int64_t>(is_ex_reg.rs2_val)); break; // BLT
                case 0x5: branch_taken = (static_cast<int64_t>(is_ex_reg.rs1_val) >= static_cast<int64_t>(is_ex_reg.rs2_val)); break; // BGE
                case 0x6: branch_taken = (is_ex_reg.rs1_val < is_ex_reg.rs2_val); break; // BLTU
                case 0x7: branch_taken = (is_ex_reg.rs1_val >= is_ex_reg.rs2_val); break; // BGEU
            }
            if (branch_taken) {
                pc_redirect_target = branch_target;
                pc_redirect_valid = true;
                flush_pipeline = true;
            }
            break;

        case 0x6F: // JAL
            result = is_ex_reg.pc + 4;
            pc_redirect_target = is_ex_reg.pc + is_ex_reg.imm;
            pc_redirect_valid = true;
            flush_pipeline = true;
            break;

        case 0x67: // JALR
            result = is_ex_reg.pc + 4;
            pc_redirect_target = (is_ex_reg.rs1_val + is_ex_reg.imm) & ~1;
            pc_redirect_valid = true;
            flush_pipeline = true;
            break;

        case 0x37: // LUI
            result = is_ex_reg.imm;
            break;

        case 0x17: // AUIPC
            result = is_ex_reg.pc + is_ex_reg.imm;
            break;
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

// =============================================================================
// MEM Stage (Memory Access)
// =============================================================================

void CPURV64P6_Cycle::MEM_stage() {
    if (!ex_mem_reg.valid) {
        mem_wb_next.valid = false;
        return;
    }

    uint64_t result = ex_mem_reg.alu_result;

    if (ex_mem_reg.mem_read) {
        // Perform load
        switch (ex_mem_reg.funct3) {
            case 0x0: result = static_cast<int64_t>(static_cast<int8_t>(mem_intf->readDataMem(ex_mem_reg.alu_result, 1))); break; // LB
            case 0x1: result = static_cast<int64_t>(static_cast<int16_t>(mem_intf->readDataMem(ex_mem_reg.alu_result, 2))); break; // LH
            case 0x2: result = static_cast<int64_t>(static_cast<int32_t>(mem_intf->readDataMem(ex_mem_reg.alu_result, 4))); break; // LW
            case 0x3: result = mem_intf->readDataMem64(ex_mem_reg.alu_result, 8); break; // LD
            case 0x4: result = mem_intf->readDataMem(ex_mem_reg.alu_result, 1); break; // LBU
            case 0x5: result = mem_intf->readDataMem(ex_mem_reg.alu_result, 2); break; // LHU
            case 0x6: result = mem_intf->readDataMem(ex_mem_reg.alu_result, 4); break; // LWU
        }
    } else if (ex_mem_reg.mem_write) {
        // Perform store
        switch (ex_mem_reg.funct3) {
            case 0x0: mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 1); break; // SB
            case 0x1: mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 2); break; // SH
            case 0x2: mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 4); break; // SW
            case 0x3: mem_intf->writeDataMem64(ex_mem_reg.alu_result, ex_mem_reg.store_data, 8); break; // SD
        }
    }

    mem_wb_next.result = result;
    mem_wb_next.rd = ex_mem_reg.rd;
    mem_wb_next.reg_write = (ex_mem_reg.rd != 0) && !ex_mem_reg.mem_write;
    mem_wb_next.valid = true;
}

// =============================================================================
// WB Stage (Writeback)
// =============================================================================

void CPURV64P6_Cycle::WB_stage() {
    if (!mem_wb_reg.valid) {
        return;
    }

    if (mem_wb_reg.reg_write && mem_wb_reg.rd != 0) {
        register_bank->setValue(mem_wb_reg.rd, mem_wb_reg.result);
        scoreboard[mem_wb_reg.rd] = false; // Clear scoreboard
    }

    stats.instructions++;
    perf->instructionsInc();
}

// =============================================================================
// IRQ Handling
// =============================================================================

bool CPURV64P6_Cycle::cpu_process_IRQ() {
    return false; // TODO: Implement interrupt handling
}

void CPURV64P6_Cycle::call_interrupt(tlm::tlm_generic_payload& m_trans, sc_core::sc_time& delay) {
    interrupt = true;
    memcpy(&int_cause, m_trans.get_data_ptr(), sizeof(BaseType));
    delay = sc_core::SC_ZERO_TIME;
}

std::uint64_t CPURV64P6_Cycle::getStartDumpAddress() {
    return register_bank->getValue(Registers<BaseType>::t0);
}

std::uint64_t CPURV64P6_Cycle::getEndDumpAddress() {
    return register_bank->getValue(Registers<BaseType>::t1);
}

void CPURV64P6_Cycle::printStats() const {
    std::cout << "  Architecture: RV64\n";
    std::cout << "  Cycles:       " << stats.cycles << "\n";
    std::cout << "  Instructions: " << stats.instructions << "\n";
    std::cout << "  CPI:          " << std::fixed << std::setprecision(2) << stats.get_cpi() << "\n";
    std::cout << "  Stalls:       " << stats.stalls << "\n";
    std::cout << "  Branches:     " << stats.branches << "\n";
    std::cout << "  Mispredicts:  " << stats.branch_mispredicts << "\n";
}

} // namespace riscv_tlm
