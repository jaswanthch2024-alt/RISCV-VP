// SPDX-License-Identifier: GPL-3.0-or-later
#include "CPU_P64_6_Cycle.h"
#include "spdlog/spdlog.h"
#include <iostream>

namespace riscv_tlm {

CPURV64P6_Cycle::CPURV64P6_Cycle(sc_core::sc_module_name const& name,
                                 BaseType PC,
                                 bool debug)
    : CPU(name, debug) {

    // Initialize Register Bank and Memory Interface
    register_bank = new Registers<BaseType>();
    mem_intf = new MemoryInterface();

    // Set Initial State
    register_bank->setPC(PC);
    // Initialize Stack Pointer (SP) to the top of memory (minus 8 bytes for alignment)
    register_bank->setValue(Registers<BaseType>::sp, 0x10000000 + Memory::SIZE - 8);
    int_cause = 0;

    // Register callback for DMI invalidations from the bus
    instr_bus.register_invalidate_direct_mem_ptr(this, &CPU::invalidate_direct_mem_ptr);

    // Initialize ISA Extensions
    base_inst = new BASE_ISA<BaseType>(0, register_bank, mem_intf);
    c_inst    = new C_extension<BaseType>(0, register_bank, mem_intf);
    m_inst    = new M_extension<BaseType>(0, register_bank, mem_intf);

    next_pc = PC; // Initialize the Next Program Counter

    // Start the main simulation thread
    SC_THREAD(cycle_thread);

    logger->info("Created CPURV64P6_Cycle (6-stage CVA6-aligned) CPU");
}

CPURV64P6_Cycle::~CPURV64P6_Cycle() {
    delete register_bank;
    delete mem_intf;
    delete base_inst;
    delete c_inst;
    delete m_inst;
}

void CPURV64P6_Cycle::set_clock(sc_core::sc_clock* c) {
    clk = c;
    if (clk) clock_period = clk->period();
}

// =============================================================================
// Process
// =============================================================================

void CPURV64P6_Cycle::cycle_thread() {
    // Initialize performance statistics
    stats.cycles = 0;
    stats.instructions = 0;

    // --- Main Simulation Loop ---
    while (true) {
        // Synchronize with the SystemC simulation kernel
        if (clk) {
            sc_core::wait(clk->posedge_event());
        } else {
             // Fallback if clock is not bound (should not happen in proper VP setup)
            sc_core::wait(clock_period);
        }

        // --- Pipeline Latch Transfer ---
        // Move data from "Next" latches to "Current" latches to simulate clock edge updates.
        issue_ex_reg = issue_ex_next;
        id_issue_reg = id_issue_next;
        fetch_id_reg = fetch_id_next;
        pcgen_fetch_reg = pcgen_fetch_next;

        // --- Execute Pipeline Stages (Reverse Order) ---
        // Executing in reverse order allows stages to read the state produced by the previous 
        // stage in the CURRENT cycle (before it gets overwritten), simulating parallel hardware.

        // 6. Commit: Retire instructions and update architectural state.
        Commit_stage();
        
        // 5. Execute: Perform ALU operations and memory address calculations.
        EX_stage();
        
        // 4. Issue: Dispatch instructions to execution units and allocate ROB entries.
        Issue_stage();
        
        // 3. Decode: Decode instructions and read register operands.
        ID_stage();
        
        // 2. Fetch: Retrieve instructions from memory.
        Fetch_stage();
        
        // 1. PC Generation: Determine the next Program Counter.
        PCGen_stage();
        
        // Update global cycle count
        stats.cycles++;

        // Periodic Heartbeat to show simulation progress
        if (stats.cycles % 5000000 == 0) {
           std::cout << "[Heartbeat] Cycles: " << std::dec << stats.cycles 
                     << " Instrs: " << stats.instructions << std::endl;
        }

        // --- Termination Logic ---
        // Stop simulation if the pipeline is completely empty and no new instructions are being fetched.
        // We add a grace period (> 100 cycles) to allow the pipeline to fill up initially.
        if (stats.cycles > 100 && 
            !pcgen_fetch_reg.valid && 
            !pcgen_fetch_next.valid &&  // Ensure no new instruction is being generated
            !fetch_id_reg.valid && 
            !id_issue_reg.valid && 
            !issue_ex_reg.valid && 
            rob.is_empty()) {
            
            std::cout << "Pipeline Empty & ROB Empty. Stopping Simulation." << std::endl;
            printStats();
            sc_core::sc_stop();
            break;
        }
    }
}

// =============================================================================
// PCGen Stage (Program Counter Generation)
// =============================================================================

void CPURV64P6_Cycle::PCGen_stage() {
    // 1. Check for Flush/Redirect from EX Stage (Highest Priority)
    // If a branch/jump misprediction or exception occurred, we must redirect the PC immediately.
    if (flush_pipeline) {
        // Redirect PC to the target address calculated in the EX stage.
        next_pc = pc_redirect_target;
        flush_pipeline = false; // Reset the flush flag after handling it.
        pc_redirect_valid = false;
        
        // Invalidate the instruction being fetched in this cycle.
        pcgen_fetch_next.valid = false;
        
        // Note: downstream stages (Fetch, ID, etc.) will see the flush signal in their next cycle 
        // or check invalid flags. We invalidate the NEXT state here to propagate the pipeline flush.
        return;
    }

    // 2. Stall Check
    // If a structural hazard or backpressure exists (e.g., ROB full), do not update the PC.
    if (stall_pcgen) {
        // Keep current state (effectively repeating the same PC for next cycle if we were outputting it)
        // or just doing nothing creates a bubble if not careful, but here 'next_pc' is persistent state.
        return;
    }

    // 3. Normal Operation
    // Pass the current PC to the Fetch stage.
    pcgen_fetch_next.pc = next_pc;
    pcgen_fetch_next.valid = true;

    // Advance PC for the NEXT cycle (standard 4-byte increment).
    next_pc += 4;
}

// =============================================================================
// Fetch Stage
// =============================================================================

void CPURV64P6_Cycle::Fetch_stage() {
    // Check for Stalls
    // If the Fetch stage is stalled (e.g., waiting for instruction memory), do not proceed.
    if (stall_fetch) {
        return; // Retain current output state
    }
    
    // Check if input from PCGen is valid
    if (!pcgen_fetch_reg.valid) {
        fetch_id_next.valid = false;
        return;
    }

    // Check for Pipeline Flush
    // Even if PCGen handled it, we double check here to be safe and ensure no trash instruction propagates.
    if (flush_pipeline) {
        fetch_id_next.valid = false;
        return;
    }

    uint64_t current_pc = pcgen_fetch_reg.pc;
    uint32_t instr = 0;

    // Attempt to Fetch Instruction from Memory
    if (fetch_instruction(current_pc, instr)) {
        // Successful Fetch: Pass instruction and PC to Decode stage.
        fetch_id_next.pc = current_pc;
        fetch_id_next.instr = instr;
        fetch_id_next.valid = true;
    } else {
        // Fetch Failed: This usually indicates a bus error or access violation.
        // We invalidate the pipeline slot here. 
        // Real hardware might trigger an Instruction Access Fault exception here.
        fetch_id_next.valid = false;
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
// ID Stage (Decode)
// =============================================================================

void CPURV64P6_Cycle::ID_stage() {
    // Check for Flush
    if (flush_pipeline) {
        id_issue_next.valid = false;
        return;
    }

    // Check for Backpressure from Issue Stage
    if (stall_issue) {
        return; // Hold current state
    }

    // Check Validity of Input
    if (!fetch_id_reg.valid) {
        id_issue_next.valid = false;
        return;
    }

    uint32_t instr = fetch_id_reg.instr;
    
    id_issue_next.pc = fetch_id_reg.pc;
    id_issue_next.instr = instr;
    id_issue_next.opcode = instr & 0x7F;
    id_issue_next.funct3 = (instr >> 12) & 0x7;
    id_issue_next.rs1 = (instr >> 15) & 0x1F;
    id_issue_next.rs2 = (instr >> 20) & 0x1F;
    id_issue_next.funct7 = (instr >> 25) & 0x7F;

    // Decode Destination Register (rd)
    // S-Type (Store) and B-Type (Branch) do not have a destination register.
    if (id_issue_next.opcode == 0x23 || id_issue_next.opcode == 0x63) {
        id_issue_next.rd = 0; 
    } else {
        id_issue_next.rd = (instr >> 7) & 0x1F;
    }

    // Decode Immediate Value (Sign-Extended)
    switch (id_issue_next.opcode) {
        case 0x13: // I-type
        case 0x03: // Load
        case 0x67: // JALR
            id_issue_next.imm = static_cast<int64_t>(static_cast<int32_t>(instr) >> 20);
            break;
        case 0x23: // S-type
            id_issue_next.imm = static_cast<int64_t>(
                static_cast<int32_t>(((instr >> 25) << 5) | ((instr >> 7) & 0x1F)) << 20 >> 20);
            break;
        case 0x63: // B-type
            id_issue_next.imm = static_cast<int64_t>(
                static_cast<int32_t>(
                    ((instr >> 31) << 12) | (((instr >> 7) & 1) << 11) |
                    (((instr >> 25) & 0x3F) << 5) | (((instr >> 8) & 0xF) << 1)) << 19 >> 19);
            break;
        case 0x37: // LUI
        case 0x17: // AUIPC
            id_issue_next.imm = static_cast<int64_t>(static_cast<int32_t>(instr & 0xFFFFF000));
            break;
        case 0x6F: // JAL
            id_issue_next.imm = static_cast<int64_t>(
                static_cast<int32_t>(
                    ((instr >> 31) << 20) | (((instr >> 12) & 0xFF) << 12) |
                    (((instr >> 20) & 1) << 11) | (((instr >> 21) & 0x3FF) << 1)) << 11 >> 11);
            break;
        case 0x73: // SYSTEM
            id_issue_next.imm = static_cast<int64_t>(static_cast<int32_t>(instr) >> 20);
            break;
        default:
            id_issue_next.imm = 0;
    }

    id_issue_next.valid = true;
}

// =============================================================================
// Issue Stage (Dispatch / Register Read)
// =============================================================================

void CPURV64P6_Cycle::Issue_stage() {
    // Reset control signals initially
    stall_pcgen = false;
    stall_fetch = false;
    stall_issue = false;

    // Check for Pipeline Flush
    if (flush_pipeline) {
        issue_ex_next.valid = false;
        return;
    }

    // Check validity of input
    if (!id_issue_reg.valid) {
        issue_ex_next.valid = false;
        return;
    }

    // --- Hazard Detection (Scoreboard) ---
    // Check if the source registers (rs1, rs2) are marked in the scoreboard.
    // If they are, it means there is a pending write to these registers from an older instruction
    // that has not yet committed. We must stall to avoid a Read-After-Write (RAW) hazard.
    if (scoreboard[id_issue_reg.rs1] || scoreboard[id_issue_reg.rs2]) {
        stall_issue = true;
        stall_fetch = true;
        stall_pcgen = true; // Backpressure all the way to PCGen
        issue_ex_next.valid = false;
        stats.stalls++; // Increment stall counter
        return;
    }

    // --- ROB Allocation ---
    // Try to allocate an entry in the Reorder Buffer (ROB).
    // The ROB ensures instructions are retired in-order even if they complete out-of-order 
    // (though this pipeline executes in-order, the ROB is used for precise exceptions and store ordering).
    int rob_idx = rob.allocate();
    if (rob_idx < 0) {
        // ROB is Full: We must stall until slots become available.
        stall_issue = true;
        stall_fetch = true;
        stall_pcgen = true;
        issue_ex_next.valid = false;
        stats.stalls++; 
        return;
    }

    // --- ROB Setup ---
    // Record instruction metadata in the allocated ROB entry.
    rob[rob_idx].pc = id_issue_reg.pc;
    rob[rob_idx].is_store = (id_issue_reg.opcode == 0x23);
    rob[rob_idx].is_branch = (id_issue_reg.opcode == 0x63 || id_issue_reg.opcode == 0x6F || id_issue_reg.opcode == 0x67);

    // --- Dispatch & Operand Read ---
    // Read operands from the register file (since we passed the scoreboard check, we know they are valid).
    issue_ex_next.pc = id_issue_reg.pc;
    issue_ex_next.rs1_val = register_bank->getValue(id_issue_reg.rs1);
    issue_ex_next.rs2_val = register_bank->getValue(id_issue_reg.rs2);
    issue_ex_next.imm = id_issue_reg.imm;
    issue_ex_next.rd = id_issue_reg.rd;
    issue_ex_next.opcode = id_issue_reg.opcode;
    issue_ex_next.funct3 = id_issue_reg.funct3;
    issue_ex_next.funct7 = id_issue_reg.funct7;
    issue_ex_next.rob_index = rob_idx; // Tag the instruction with its ROB index
    issue_ex_next.valid = true;

    // --- Scoreboard Update ---
    // Mark the destination register as pending.
    if (id_issue_reg.rd != 0) {
        scoreboard[id_issue_reg.rd] = true;
    }
}

// =============================================================================
// EX Stage (Execute & Memory Access)
// =============================================================================

void CPURV64P6_Cycle::EX_stage() {
    // Note: The EX stage in this model does NOT latch to a "next" stage like EX->MEM. 
    // Instead, it completes execution and writes the result directly to the ROB (for registers) 
    // or the Store Buffer (for memory stores).
    
    if (!issue_ex_reg.valid) {
        return;
    }

    uint64_t result = 0;
    bool branch_taken = false;
    uint64_t branch_target = 0;
    
    // 1. Execute ALU Operations
    switch (issue_ex_reg.opcode) {
        case 0x33: // R-type (Register-Register)
            switch (issue_ex_reg.funct3) {
                case 0x0: 
                    if (issue_ex_reg.funct7 == 0x20) result = issue_ex_reg.rs1_val - issue_ex_reg.rs2_val;
                    else result = issue_ex_reg.rs1_val + issue_ex_reg.rs2_val;
                    break;
                case 0x1: result = issue_ex_reg.rs1_val << (issue_ex_reg.rs2_val & 0x3F); break;
                case 0x2: result = (static_cast<int64_t>(issue_ex_reg.rs1_val) < static_cast<int64_t>(issue_ex_reg.rs2_val)); break;
                case 0x3: result = (issue_ex_reg.rs1_val < issue_ex_reg.rs2_val); break;
                case 0x4: result = issue_ex_reg.rs1_val ^ issue_ex_reg.rs2_val; break;
                case 0x5: 
                    if (issue_ex_reg.funct7 == 0x20) result = static_cast<int64_t>(issue_ex_reg.rs1_val) >> (issue_ex_reg.rs2_val & 0x3F);
                    else result = issue_ex_reg.rs1_val >> (issue_ex_reg.rs2_val & 0x3F);
                    break;
                case 0x6: result = issue_ex_reg.rs1_val | issue_ex_reg.rs2_val; break;
                case 0x7: result = issue_ex_reg.rs1_val & issue_ex_reg.rs2_val; break;
            }
            break;
        case 0x13: // I-type ALU (Immediate-Register)
             switch (issue_ex_reg.funct3) {
                case 0x0: result = issue_ex_reg.rs1_val + issue_ex_reg.imm; break; 
                case 0x2: result = (static_cast<int64_t>(issue_ex_reg.rs1_val) < issue_ex_reg.imm); break; 
                case 0x3: result = (issue_ex_reg.rs1_val < static_cast<uint64_t>(issue_ex_reg.imm)); break; 
                case 0x4: result = issue_ex_reg.rs1_val ^ issue_ex_reg.imm; break; 
                case 0x6: result = issue_ex_reg.rs1_val | issue_ex_reg.imm; break; 
                case 0x7: result = issue_ex_reg.rs1_val & issue_ex_reg.imm; break; 
                case 0x1: result = issue_ex_reg.rs1_val << (issue_ex_reg.imm & 0x3F); break; 
                case 0x5: 
                    if ((issue_ex_reg.imm & 0x400) != 0) result = static_cast<int64_t>(issue_ex_reg.rs1_val) >> (issue_ex_reg.imm & 0x3F);
                    else result = issue_ex_reg.rs1_val >> (issue_ex_reg.imm & 0x3F);
                    break;
            }
            break;
        case 0x37: result = issue_ex_reg.imm; break; // LUI
        case 0x17: result = issue_ex_reg.pc + issue_ex_reg.imm; break; // AUIPC
        case 0x6F: // JAL
            result = issue_ex_reg.pc + 4;
            branch_target = issue_ex_reg.pc + issue_ex_reg.imm;
            branch_taken = true;
            break;
        case 0x67: // JALR
            result = issue_ex_reg.pc + 4;
            branch_target = (issue_ex_reg.rs1_val + issue_ex_reg.imm) & ~1;
            branch_taken = true;
            break;
        case 0x63: // Branch
            stats.branches++; // Count Branch
            branch_target = issue_ex_reg.pc + issue_ex_reg.imm;
             switch (issue_ex_reg.funct3) {
                case 0x0: branch_taken = (issue_ex_reg.rs1_val == issue_ex_reg.rs2_val); break;
                case 0x1: branch_taken = (issue_ex_reg.rs1_val != issue_ex_reg.rs2_val); break;
                case 0x4: branch_taken = (static_cast<int64_t>(issue_ex_reg.rs1_val) < static_cast<int64_t>(issue_ex_reg.rs2_val)); break;
                case 0x5: branch_taken = (static_cast<int64_t>(issue_ex_reg.rs1_val) >= static_cast<int64_t>(issue_ex_reg.rs2_val)); break;
                case 0x6: branch_taken = (issue_ex_reg.rs1_val < issue_ex_reg.rs2_val); break;
                case 0x7: branch_taken = (issue_ex_reg.rs1_val >= issue_ex_reg.rs2_val); break;
            }
            break;
    }

    // 2. Load/Store Execution (LSU)
    // Perform memory access synchronously.
    if (issue_ex_reg.opcode == 0x03) { // Load
        uint64_t addr = issue_ex_reg.rs1_val + issue_ex_reg.imm;
        switch (issue_ex_reg.funct3) {
            case 0x0: result = static_cast<int64_t>(static_cast<int8_t>(mem_intf->readDataMem(addr, 1))); break;
            case 0x1: result = static_cast<int64_t>(static_cast<int16_t>(mem_intf->readDataMem(addr, 2))); break;
            case 0x2: result = static_cast<int64_t>(static_cast<int32_t>(mem_intf->readDataMem(addr, 4))); break;
            case 0x3: result = mem_intf->readDataMem64(addr, 8); break;
            case 0x4: result = mem_intf->readDataMem(addr, 1); break;
            case 0x5: result = mem_intf->readDataMem(addr, 2); break;
            case 0x6: result = mem_intf->readDataMem(addr, 4); break;
        }
    } else if (issue_ex_reg.opcode == 0x23) { // Store
        uint64_t addr = issue_ex_reg.rs1_val + issue_ex_reg.imm;
        int size = 0;
        switch (issue_ex_reg.funct3) {
            case 0x0: size = 1; break;
            case 0x1: size = 2; break;
            case 0x2: size = 4; break;
            case 0x3: size = 8; break;
        }
        // Instead of writing to memory immediately, add it to the Store Buffer.
        // It will be committed to memory in the Commit stage.
        store_buffer.add_store(addr, issue_ex_reg.rs2_val, size, issue_ex_reg.rob_index);
    }

    // 3. Handle Branch Redirection
    // If a branch is taken, flush the pipeline and set the redirect target.
    if (branch_taken) {
         pc_redirect_target = branch_target;
         pc_redirect_valid = true;
         flush_pipeline = true;
    }
    
    // 4. Handle System (ECALL)
    if (issue_ex_reg.opcode == 0x73) {
        if (issue_ex_reg.funct3 == 0 && issue_ex_reg.imm == 0) {
            uint64_t syscall_num = register_bank->getValue(17);
            if (syscall_num == 93) { // Exit
                 sc_core::sc_stop();
            }
        }
    }

    // 5. Complete Instruction in ROB (State Update)
    // Mark the instruction as "complete" in the ROB, making it ready for retirement.
    if (issue_ex_reg.rob_index >= 0) {
        rob.complete(issue_ex_reg.rob_index, result, issue_ex_reg.rd);
    }
}

// =============================================================================
// Commit Stage (Architectural Commit)
// =============================================================================

void CPURV64P6_Cycle::Commit_stage() {
    // Check if the ROB has a completed instruction at its head
    if (rob.head_ready()) {
        const ROBEntry& entry = rob.get_head();
        
        // 1. Commit Store Operations
        // If the instruction was a store, it has been waiting in the Store Buffer.
        // We now finally write it to actual memory.
        if (entry.is_store) {
            uint64_t addr, data;
            int size;
            if (store_buffer.commit_store(rob.get_head_index(), addr, data, size)) {
                 if (size == 8) mem_intf->writeDataMem64(addr, data, size);
                 else mem_intf->writeDataMem(addr, static_cast<uint32_t>(data), size);
            }
        }
        
        // 2. Commit Register Results (Architectural Update)
        // Write the result to the register file and release the scoreboard lock.
        // This makes the result visible to new instructions (and clears the hazard).
        if (entry.dest_reg != 0) {
            register_bank->setValue(entry.dest_reg, entry.result);
            scoreboard[entry.dest_reg] = false; // Release Lock
        }
        
        // 3. Update Performance Statistics
        stats.instructions++;
        if (perf) perf->instructionsInc();

        // 4. Retire Instruction
        // Remove the instruction from the ROB/Pipeline.
        rob.retire();
    }
}

// =============================================================================
// Helpers / Boilerplate
// =============================================================================

bool CPURV64P6_Cycle::cpu_process_IRQ() {
    return false;
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
    std::cout << "  Architecture: RV64 (CVA6 6-Stage Aligned)\n";
    std::cout << "  Cycles:       " << stats.cycles << "\n";
    std::cout << "  Instructions: " << stats.instructions << "\n";
    std::cout << "  CPI:          " << std::fixed << std::setprecision(2) << stats.get_cpi() << "\n";
    std::cout << "  Stalls:       " << stats.stalls << "\n";
    std::cout << "  Branches:     " << stats.branches << "\n";
}

} // namespace riscv_tlm
