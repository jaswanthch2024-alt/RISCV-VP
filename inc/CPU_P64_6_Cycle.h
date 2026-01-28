// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file CPU_P64_6_Cycle.h
 * @brief 6-Stage Pipelined RISC-V 64-bit CPU - Cycle-Accurate Timing Model
 * 
 * Pipeline Stages:
 *   PC -> IF -> ID -> IS -> EX -> MEM -> WB
 */
#pragma once
#ifndef CPU_P64_6_CYCLE_H
#define CPU_P64_6_CYCLE_H

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"

#include "CPU.h"
#include "Instruction.h"
#include "MemoryInterface.h"
#include "Registers.h"
#include "Memory.h"
#include "BASE_ISA.h"
#include "C_extension.h"
#include "M_extension.h"
#include "Performance.h"
#include "ROB.h"
#include "StoreBuffer.h"

namespace riscv_tlm {

class CPURV64P6_Cycle : public CPU {
public:
    using BaseType = std::uint64_t;

    SC_HAS_PROCESS(CPURV64P6_Cycle);

    CPURV64P6_Cycle(sc_core::sc_module_name const& name, BaseType PC, bool debug);
    ~CPURV64P6_Cycle() override;

    void set_clock(sc_core::sc_clock* c) override;
    bool CPU_step() override { return false; }
    bool cpu_process_IRQ() override;
    void call_interrupt(tlm::tlm_generic_payload& m_trans, sc_core::sc_time& delay) override;

    std::uint64_t getStartDumpAddress() override;
    std::uint64_t getEndDumpAddress() override;
    bool isPipelined() const override { return true; }

    void printStats() const;

private:
    // =========================================================================
    // Components
    // =========================================================================
    Registers<BaseType>*    register_bank{nullptr};
    
    BASE_ISA<BaseType>*     base_inst{nullptr};
    C_extension<BaseType>*  c_inst{nullptr};
    M_extension<BaseType>*  m_inst{nullptr};

    BaseType int_cause{0};
    
    sc_core::sc_clock* clk{nullptr};
    sc_core::sc_time clock_period{10, sc_core::SC_NS};

    // =========================================================================
    // Pipeline Latches
    // =========================================================================
    
    // --- Pipeline Latches ---
    // These structures hold the state transferred between pipeline stages on each clock cycle.
    
    // PCGen -> Fetch Latch
    // Holds the next program counter to be fetched.
    struct PCGen_Fetch_Latch {
        uint64_t pc{0};    // Address to fetch from
        bool valid{false}; // Start fetching?
    } pcgen_fetch_reg, pcgen_fetch_next;

    // Fetch -> ID Latch
    // Holds the fetched instruction and its PC.
    struct Fetch_ID_Latch {
        uint64_t pc{0};
        uint32_t instr{0}; // Raw instruction data
        bool valid{false};
    } fetch_id_reg, fetch_id_next;

    // ID -> Issue Latch
    // Holds decoded instruction details ready for dispatch.
    struct ID_Issue_Latch {
        uint64_t pc{0};
        uint32_t instr{0};
        uint8_t rd{0}, rs1{0}, rs2{0}; // Register indices
        int64_t imm{0};                // Decoded immediate
        uint8_t opcode{0};
        uint8_t funct3{0};
        uint8_t funct7{0};
        bool valid{0};
    } id_issue_reg, id_issue_next;

    // Issue -> EX Latch
    // Holds dispatched instruction with operands read from register bank.
    // Also contains the allocated ROB index for retirement.
    struct Issue_EX_Latch {
        uint64_t pc{0};
        uint64_t rs1_val{0};
        uint64_t rs2_val{0};
        int64_t imm{0};
        uint8_t rd{0};
        uint8_t opcode{0};
        uint8_t funct3{0};
        uint8_t funct7{0};
        int rob_index{-1};   // Index in Reorder Buffer (for tracking completion)
        bool valid{false};
    } issue_ex_reg, issue_ex_next;

    // EX -> Commit (ROB/Architectural Update Interface)
    // Conceptually, EX writes to ROB. Commit reads from ROB.
    // We can use a latch to simulate the "Writeback" port to ROB if needed, 
    // or just let EX update ROB directly.
    // For this model, EX will write to ROB, and Commit will retire from ROB.

    // =========================================================================
    // Control & State
    // =========================================================================
    uint64_t next_pc{0};           // Next Program Counter (calculated by PCGen)
    
    // Stall Signals affecting various stages
    bool stall_pcgen{false};
    bool stall_fetch{false};
    bool stall_issue{false};
    
    // Flush Signal (e.g. for Branch Misprediction)
    bool flush_pipeline{false};
    uint64_t pc_redirect_target{0};
    bool pc_redirect_valid{false};
 
    // Scoreboard for hazard detection
    // Tracks registers pending writeback.
    bool scoreboard[32]{false};

    // =========================================================================
    // Statistics
    // =========================================================================
    struct Stats {
        uint64_t cycles{0};
        uint64_t instructions{0};
        uint64_t stalls{0};
        uint64_t branches{0};
        uint64_t branch_mispredicts{0};
        
        double get_cpi() const { return instructions > 0 ? (double)cycles / instructions : 0; }
    };
    Stats stats;

    // =========================================================================
    // Out-of-Order Execution Components
    // =========================================================================
    
    // Reorder Buffer (ROB)
    // Ensures instructions are retired in-order even if they complete execution out-of-order.
    // Handles precise exceptions and provides temporary storage for results before commit.
    ReorderBuffer<32> rob; 
    
    // Store Buffer
    // Buffers memory writes until the store instruction is ready to commit.
    // Prevents speculative stores from modifying memory irreversibly.
    StoreBuffer<8> store_buffer;

    // =========================================================================
    // Stage Methods
    // =========================================================================
    void PCGen_stage();
    void Fetch_stage();
    void ID_stage();
    void Issue_stage();
    void EX_stage();     // Includes Memory Access (LSU)
    void Commit_stage(); // Handles Retirement

    void cycle_thread();

    // =========================================================================
    // Helpers
    // =========================================================================
    bool fetch_instruction(uint64_t addr, uint32_t& data);
    // DMI uses base class members: dmi_ptr_valid, dmi_ptr (inherited from CPU)
    uint64_t dmi_start_addr{0};
    uint64_t dmi_end_addr{0};
};

} // namespace riscv_tlm

#endif // CPU_P64_6_CYCLE_H