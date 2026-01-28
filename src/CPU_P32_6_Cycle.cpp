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

    // Initialize the register bank and memory interface
    register_bank = new Registers<BaseType>();
    mem_intf = new MemoryInterface();

    // Set the initial Program Counter (PC) and Stack Pointer (SP)
    register_bank->setPC(PC);
    register_bank->setValue(Registers<BaseType>::sp, (Memory::SIZE / 4) - 1);
    int_cause = 0;

    // Register the callback for Direct Memory Interface (DMI) invalidation
    instr_bus.register_invalidate_direct_mem_ptr(this, &CPU::invalidate_direct_mem_ptr);

    // Initialize the ISA extension handlers
    base_inst = new BASE_ISA<BaseType>(0, register_bank, mem_intf);
    c_inst    = new C_extension<BaseType>(0, register_bank, mem_intf);
    m_inst    = new M_extension<BaseType>(0, register_bank, mem_intf);

    pc_register = PC;

    // Start the main simulation thread
    SC_THREAD(cycle_thread);

    logger->info("Created CPURV32P6_Cycle (6-stage cycle-accurate) CPU");
}

CPURV32P6_Cycle::~CPURV32P6_Cycle() {
    delete register_bank;
    delete mem_intf;
    delete base_inst;
    delete c_inst;
    delete m_inst;
    // Note: m_qk is allocated by base CPU class but not used by cycle-accurate model
    // Base class destructor should handle cleanup if needed
}

void CPURV32P6_Cycle::set_clock(sc_core::sc_clock* c) {
    clk = c;
    if (clk) clock_period = clk->period();
}

void CPURV32P6_Cycle::cycle_thread() {
    std::cout << "[DEBUG] Paying cycle_thread started" << std::endl;
    
    // --- Reset Logic ---
    // Clear all general-purpose registers to ensure a clean state at startup.
    for (int i = 0; i < 32; ++i) {
        register_bank->setValue(i, 0);
    }
    // Set the Stack Pointer (x2) to the top of the RAM.
    // We use 0x2FFFFF00 to ensure we are safely within the bounds of the 512MB memory 
    // (which ends at 0x30000000).
    register_bank->setValue(2, 0x2FFFFF00); 

    // --- Main Simulation Loop ---
    while (true) {
        // Update simulation statistics
        stats.cycles++;

        // Synchronize with the SystemC simulation kernel.
        // If a clock is defined, wait for the next positive edge.
        // Otherwise, wait for the defined clock period.
        if (clk) {
             sc_core::wait(clk->posedge_event());
        }
        else {
             sc_core::wait(clock_period);
             std::cout << "[DEBUG] Wait period" << std::endl;
        }

        // --- Pipeline Latch Transfer ---
        // Move data from the "next" state latches to the "current" state latches for the new cycle.
        // This simulates the clock edge updating the pipeline registers.
        mem_wb_reg = mem_wb_next;
        ex_mem_reg = ex_mem_next;
        is_ex_reg = is_ex_next;
        id_is_reg = id_is_next;
        
        // Update the Instruction Fetch to Instruction Decode latch only if the fetch stage is not stalled.
        if (!stall_fetch) {
            if_id_reg = if_id_next;
        }

        // --- Execute Pipeline Stages ---
        // We execute stages in reverse order (WB -> IF) to simulate parallel execution logic within a sequential function.
        // This ensures that a stage reads values produced by the previous stage in the *current* cycle's latch state,
        // simulating simultaneous hardware operation.

        // Write Back (WB): Commit results to registers.
        WB_stage();
        
        // Memory Access (MEM): Perform load/store operations.
        MEM_stage();
        
        // Execute (EX): Perform ALU operations and resolve branches.
        EX_stage();
        
        // Instruction Issue (IS): Check for hazards and issue to EX.
        IS_stage();
        
        // Instruction Decode (ID): Decode instruction and generate control signals.
        ID_stage();
        
        // Instruction Fetch (IF): Fetch the next instruction from memory.
        IF_stage();
    }
}

// Logic to select the next Program Counter (PC)
void CPURV32P6_Cycle::pc_select() {
    // Priority 1: Handle Branch/Jump Redirection
    // If a branch was taken or a jump occurred in the EX stage, update the PC to the target address.
    if (pc_redirect_valid) {
        pc_register = pc_redirect_target;
        pc_redirect_valid = false;
        flush_pipeline = false; // We have handled the redirect, so we can stop flushing.
        return;
    }
    
    // Priority 2: Valid Stall
    // If the pipeline is stalled (e.g., due to a data hazard), do not change the PC.
    if (stall_fetch) return;
    
    // Default: Sequential Execution
    // Move to the next instruction address (4 bytes ahead).
    pc_register += 4;
}

void CPURV32P6_Cycle::IF_stage() {
    // 1. Check for DMA Conflicts
    // If the Direct Memory Access (DMA) unit is using the bus, we must wait (stall) 
    // until it releases the bus to avoid bus contention.
    while (riscv_tlm::peripherals::DMA::is_in_flight()) {
        if (clk) wait(clk->posedge_event());
        else wait(clock_period);
    }

    // 2. Capture the current PC to fetch
    uint32_t current_pc = pc_register;

    // 3. Compute the next PC for the *following* cycle
    pc_select();

    // 4. Handle Pipeline Flush
    // If a flush signal is active (e.g., from a mispredicted branch), invalidate the fetched instruction.
    if (flush_pipeline) {
        if_id_next.valid = false;
        return;
    }

    // 5. Fetch Instruction
    uint32_t instr = 0;
    if (fetch_instruction(current_pc, instr)) {
        // Successful fetch: Pass the instruction and PC to the ID stage.
        if_id_next.pc = current_pc;
        if_id_next.instr = instr;
        if_id_next.valid = true;
    } else {
        // Fetch Error: Typically means accessing invalid memory (Segfault). Stop simulation.
        std::cout << "[Sim] Error: Fetch failed at PC=" << std::hex << current_pc << std::dec << " (Out of bounds). Stopping." << std::endl;
        sc_core::sc_stop();
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
    // Handle flushes and stalls
    if (flush_pipeline) { id_is_next.valid = false; return; }
    if (stall_fetch) return;
    
    // If the incoming instruction is not valid, propagate the invalid state.
    if (!if_id_reg.valid) {
        id_is_next.valid = false;
        return;
    }

    uint32_t instr = if_id_reg.instr;
    
    // Pass PC and Instruction to proper fields
    id_is_next.pc = if_id_reg.pc;
    id_is_next.instr = instr;
    
    // --- Decode Fields ---
    id_is_next.opcode = instr & 0x7F;
    
    // Destination Register (rd)
    // Note: S-Type (Store) and B-Type (Branch) instructions do NOT write to a register, so rd is 0.
    if (id_is_next.opcode == 0x23 || id_is_next.opcode == 0x63) {
        id_is_next.rd = 0;
    } else {
        id_is_next.rd = (instr >> 7) & 0x1F;
    }
    
    // Source Registers (rs1, rs2) and Function Codes (funct3, funct7)
    id_is_next.funct3 = (instr >> 12) & 0x7;
    id_is_next.rs1 = (instr >> 15) & 0x1F;
    id_is_next.rs2 = (instr >> 20) & 0x1F;
    id_is_next.funct7 = (instr >> 25) & 0x7F;

    // --- Immediate Generation ---
    // Extract and sign-extend the immediate value based on the instruction type.
    switch (id_is_next.opcode) {
        case 0x13: case 0x03: case 0x67: // I-Type
            id_is_next.imm = static_cast<int32_t>(instr) >> 20;
            break;
        case 0x23: // S-Type
            id_is_next.imm = static_cast<int32_t>(((instr >> 25) << 5) | ((instr >> 7) & 0x1F)) << 20 >> 20;
            break;
        case 0x63: // B-Type
            id_is_next.imm = static_cast<int32_t>(
                ((instr >> 31) << 12) | (((instr >> 7) & 1) << 11) |
                (((instr >> 25) & 0x3F) << 5) | (((instr >> 8) & 0xF) << 1)) << 19 >> 19;
            break;
        case 0x37: case 0x17: // U-Type
            id_is_next.imm = static_cast<int32_t>(instr & 0xFFFFF000);
            break;
        case 0x6F: // J-Type
            id_is_next.imm = static_cast<int32_t>(
                ((instr >> 31) << 20) | (((instr >> 12) & 0xFF) << 12) |
                (((instr >> 20) & 1) << 11) | (((instr >> 21) & 0x3FF) << 1)) << 11 >> 11;
            break;
        default:
            id_is_next.imm = 0;
    }
    
    // Mark the decoded instruction as valid for the next stage.
    id_is_next.valid = true;
}

void CPURV32P6_Cycle::IS_stage() {
    if (flush_pipeline) { is_ex_next.valid = false; return; }
    
    // Check if we have a valid instruction from the decode stage.
    if (!id_is_reg.valid) {
        is_ex_next.valid = false;
        return;
    }

    // --- Hazard Detection (Scoreboarding) ---
    // Check if any of the source registers (rs1, rs2) are currently pending a write from a later stage.
    // If so, we have a data hazard.
    if (scoreboard[id_is_reg.rs1] || scoreboard[id_is_reg.rs2]) {
        // Stall Logic:
        // 1. Send a "bubble" (nop/invalid) to the Execute stage.
        is_ex_next.valid = false; 
        
        // 2. Keep the current instruction in the Issue stage (hold the latch).
        id_is_next = id_is_reg;
        
        // 3. Signal the fetch stage to stop fetching new instructions.
        stall_fetch = true; 
        return;
    }
    
    // No stall required: Clear the stall signal (unless forced by another mechanism).
    stall_fetch = false; 

    // --- Operand Fetch ---
    // Read the values from the register bank and pass them to the Execute stage.
    is_ex_next.pc = id_is_reg.pc;
    is_ex_next.rs1_val = register_bank->getValue(id_is_reg.rs1);
    is_ex_next.rs2_val = register_bank->getValue(id_is_reg.rs2);
    is_ex_next.imm = id_is_reg.imm;
    is_ex_next.rd = id_is_reg.rd;
    is_ex_next.opcode = id_is_reg.opcode;
    is_ex_next.funct3 = id_is_reg.funct3;
    is_ex_next.funct7 = id_is_reg.funct7;
    is_ex_next.valid = true;

    // --- Update Scoreboard ---
    // Mark the destination register as "pending write" to prevent future instructions from 
    // reading it until the write-back is complete.
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

    // Execute the operation based on the opcode
    switch (is_ex_reg.opcode) {
        case 0x33: // R-type Instructions (Register-Register)
            switch (is_ex_reg.funct3) {
                case 0x0: // ADD / SUB
                    if (is_ex_reg.funct7 == 0x20) result = is_ex_reg.rs1_val - is_ex_reg.rs2_val;
                    else result = is_ex_reg.rs1_val + is_ex_reg.rs2_val;
                    break;
                case 0x1: // SLL (Shift Left Logical)
                    result = is_ex_reg.rs1_val << (is_ex_reg.rs2_val & 0x1F); break;
                case 0x2: // SLT (Set Less Than)
                    result = (static_cast<int32_t>(is_ex_reg.rs1_val) < static_cast<int32_t>(is_ex_reg.rs2_val)); break;
                case 0x3: // SLTU (Set Less Than Unsigned)
                    result = (is_ex_reg.rs1_val < is_ex_reg.rs2_val); break;
                case 0x4: // XOR
                    result = is_ex_reg.rs1_val ^ is_ex_reg.rs2_val; break;
                case 0x5: // SRL / SRA (Shift Right Logical / Arithmetic)
                    if (is_ex_reg.funct7 == 0x20) result = static_cast<int32_t>(is_ex_reg.rs1_val) >> (is_ex_reg.rs2_val & 0x1F);
                    else result = is_ex_reg.rs1_val >> (is_ex_reg.rs2_val & 0x1F);
                    break;
                case 0x6: // OR
                    result = is_ex_reg.rs1_val | is_ex_reg.rs2_val; break;
                case 0x7: // AND
                    result = is_ex_reg.rs1_val & is_ex_reg.rs2_val; break;
            }
            break;

        case 0x13: // I-type Instructions (Immediate-Register)
            switch (is_ex_reg.funct3) {
                case 0x0: // ADDI
                    result = is_ex_reg.rs1_val + is_ex_reg.imm; break;
                case 0x2: // SLTI
                    result = (static_cast<int32_t>(is_ex_reg.rs1_val) < is_ex_reg.imm); break;
                case 0x3: // SLTIU
                    result = (is_ex_reg.rs1_val < static_cast<uint32_t>(is_ex_reg.imm)); break;
                case 0x4: // XORI
                    result = is_ex_reg.rs1_val ^ is_ex_reg.imm; break;
                case 0x6: // ORI
                    result = is_ex_reg.rs1_val | is_ex_reg.imm; break;
                case 0x7: // ANDI
                    result = is_ex_reg.rs1_val & is_ex_reg.imm; break;
                case 0x1: // SLLI
                    result = is_ex_reg.rs1_val << (is_ex_reg.imm & 0x1F); break;
                case 0x5: // SRLI / SRAI
                    if ((is_ex_reg.imm & 0x400) != 0) result = static_cast<int32_t>(is_ex_reg.rs1_val) >> (is_ex_reg.imm & 0x1F);
                    else result = is_ex_reg.rs1_val >> (is_ex_reg.imm & 0x1F);
                    break;
            }
            break;

        case 0x03: // Load Instructions
            result = is_ex_reg.rs1_val + is_ex_reg.imm; // Calculate effective address
            mem_read = true; 
            break;

        case 0x23: // Store Instructions
            result = is_ex_reg.rs1_val + is_ex_reg.imm; // Calculate effective address
            mem_write = true; 
            break;

        case 0x63: // Branch Instructions
            branch_target = is_ex_reg.pc + is_ex_reg.imm;
            switch (is_ex_reg.funct3) {
                case 0x0: // BEQ
                    branch_taken = (is_ex_reg.rs1_val == is_ex_reg.rs2_val); break;
                case 0x1: // BNE
                    branch_taken = (is_ex_reg.rs1_val != is_ex_reg.rs2_val); break;
                case 0x4: // BLT
                    branch_taken = (static_cast<int32_t>(is_ex_reg.rs1_val) < static_cast<int32_t>(is_ex_reg.rs2_val)); break;
                case 0x5: // BGE
                    branch_taken = (static_cast<int32_t>(is_ex_reg.rs1_val) >= static_cast<int32_t>(is_ex_reg.rs2_val)); break;
                case 0x6: // BLTU
                    branch_taken = (is_ex_reg.rs1_val < is_ex_reg.rs2_val); break;
                case 0x7: // BGEU
                    branch_taken = (is_ex_reg.rs1_val >= is_ex_reg.rs2_val); break;
            }
            // If branch is taken, redirect PC and flush next cycle
            if (branch_taken) {
                pc_redirect_target = branch_target;
                pc_redirect_valid = true;
                flush_pipeline = true;
            }
            break;

        case 0x6F: // JAL (Jump and Link)
            result = is_ex_reg.pc + 4; // Save return address
            pc_redirect_target = is_ex_reg.pc + is_ex_reg.imm; 
            pc_redirect_valid = true; 
            flush_pipeline = true; 
            break;

        case 0x67: // JALR (Jump and Link Register)
            result = is_ex_reg.pc + 4; // Save return address
            pc_redirect_target = (is_ex_reg.rs1_val + is_ex_reg.imm) & ~1; // Target is rs1 + imm, LSB masked to 0
            pc_redirect_valid = true; 
            flush_pipeline = true; 
            break;

        case 0x37: // LUI (Load Upper Immediate)
            result = is_ex_reg.imm; 
            break;

        case 0x17: // AUIPC (Add Upper Immediate to PC)
            result = is_ex_reg.pc + is_ex_reg.imm; 
            break;

        case 0x73: // SYSTEM (ECALL, etc.)
            if (is_ex_reg.funct3 == 0 && is_ex_reg.imm == 0) {
                 // ECALL: Get syscall number from A7 (x17)
                 uint32_t syscall_num = register_bank->getValue(17);
                 if (syscall_num == 93 || syscall_num == 1) { // exit (93) or (1)
                     std::cout << "ECALL: exit detected. Stopping simulation." << std::endl;
                     sc_core::sc_stop();
                 } else if (syscall_num == 64) { // sys_write
                     uint32_t fd = register_bank->getValue(10);
                     uint32_t ptr = register_bank->getValue(11);
                     uint32_t len = register_bank->getValue(12);
                     if (fd == 1) { // stdout
                         for (uint32_t i = 0; i < len; i++) {
                             char c = static_cast<char>(mem_intf->readDataMem(ptr + i, 1));
                             std::cout << c;
                         }
                         std::cout << std::flush;
                     }
                 }
            }
            break;
    }

    // Forward results to MEM stage
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

    // Handle Memory Read Operations
    if (ex_mem_reg.mem_read) {
        switch (ex_mem_reg.funct3) {
            case 0x0: // LB (Load Byte)
                result = static_cast<int32_t>(static_cast<int8_t>(mem_intf->readDataMem(ex_mem_reg.alu_result, 1))); 
                break;
            case 0x1: // LH (Load Halfword)
                result = static_cast<int32_t>(static_cast<int16_t>(mem_intf->readDataMem(ex_mem_reg.alu_result, 2))); 
                break;
            case 0x2: // LW (Load Word)
                result = mem_intf->readDataMem(ex_mem_reg.alu_result, 4); 
                break;
            case 0x4: // LBU (Load Byte Unsigned)
                result = mem_intf->readDataMem(ex_mem_reg.alu_result, 1); 
                break;
            case 0x5: // LHU (Load Halfword Unsigned)
                result = mem_intf->readDataMem(ex_mem_reg.alu_result, 2); 
                break;
        }
    } 
    // Handle Memory Write Operations
    else if (ex_mem_reg.mem_write) {
        switch (ex_mem_reg.funct3) {
            case 0x0: // SB (Store Byte)
                mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 1); 
                break;
            case 0x1: // SH (Store Halfword)
                mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 2); 
                break;
            case 0x2: // SW (Store Word)
                mem_intf->writeDataMem(ex_mem_reg.alu_result, ex_mem_reg.store_data, 4); 
                break;
        }
    }

    // Pass results to Write Back stage
    mem_wb_next.result = result;
    mem_wb_next.rd = ex_mem_reg.rd;
    // We only write to the register if the destination is not x0 (hardwired to 0) 
    // and this is not a store instruction.
    mem_wb_next.reg_write = (ex_mem_reg.rd != 0) && !ex_mem_reg.mem_write;
    mem_wb_next.valid = true;
}

void CPURV32P6_Cycle::WB_stage() {
    // If the instruction coming from MEM is invalid, do nothing.
    if (!mem_wb_reg.valid) return;
    
    // Perform the actual register write
    if (mem_wb_reg.reg_write && mem_wb_reg.rd != 0) {
        register_bank->setValue(mem_wb_reg.rd, mem_wb_reg.result);
        
        // Critical: Release the lock on the destination register in the scoreboard
        // effectively indicating that the dependency is resolved.
        scoreboard[mem_wb_reg.rd] = false;
    }
    
    // Increment stats for retired instructions
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
