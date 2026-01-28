// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file CPU_P32_6_Cycle.h
 * @brief 6-Stage Pipelined RISC-V 32-bit CPU - Cycle-Accurate Timing Model
 * 
 * Pipeline Stages:
 *   PC -> IF -> ID -> IS -> EX -> MEM -> WB
 */
#pragma once
#ifndef CPU_P32_6_CYCLE_H
#define CPU_P32_6_CYCLE_H

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

class CPURV32P6_Cycle : public CPU {
public:
    using BaseType = std::uint32_t;

    SC_HAS_PROCESS(CPURV32P6_Cycle);

    CPURV32P6_Cycle(sc_core::sc_module_name const& name, BaseType PC, bool debug);
    ~CPURV32P6_Cycle() override;

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
    // Each latch has a `_reg` (current cycle output) and `_next` (next cycle input) instance.

    // IF -> ID Latch (Fetch to Decode)
    // Holds the instruction fetched from memory and its PC.
    struct IF_ID_Latch {
        uint32_t pc{0};     // Program Counter of the instruction
        uint32_t instr{0};  // Raw instruction word
        bool valid{false};  // Validity flag (false if flushed or bubble)
    } if_id_reg, if_id_next;

    // ID -> IS Latch (Decode to Issue)
    // Holds the decoded instruction details and extracted operands.
    struct ID_IS_Latch {
        uint32_t pc{0};
        uint32_t instr{0};
        uint8_t rd{0}, rs1{0}, rs2{0}; // Register indices
        int32_t imm{0};                // Sign-extended immediate value
        uint8_t opcode{0};
        uint8_t funct3{0};
        uint8_t funct7{0};
        bool valid{false};
    } id_is_reg, id_is_next;

    // IS -> EX Latch (Issue to Execute)
    // Holds the values of the operands needed for execution.
    // At this point, registers have been read and hazards resolved.
    struct IS_EX_Latch {
        uint32_t pc{0};
        uint32_t rs1_val{0}; // Value of Source Register 1
        uint32_t rs2_val{0}; // Value of Source Register 2
        int32_t imm{0};
        uint8_t rd{0};
        uint8_t opcode{0};
        uint8_t funct3{0};
        uint8_t funct7{0};
        bool valid{false};
    } is_ex_reg, is_ex_next;

    // EX -> MEM Latch (Execute to Memory)
    // Holds the ALU result (which might be an address or data) and control signals for memory access.
    struct EX_MEM_Latch {
        uint32_t pc{0};
        uint32_t alu_result{0};    // Result of ALU operation or Effective Address
        uint32_t store_data{0};    // Data to be written to memory (if store)
        uint8_t rd{0};
        uint8_t funct3{0};
        bool mem_read{false};      // Control signal: Read from memory?
        bool mem_write{false};     // Control signal: Write to memory?
        bool branch_taken{false};  // Was a branch taken?
        uint32_t branch_target{0}; // Where to branch to?
        bool valid{false};
    } ex_mem_reg, ex_mem_next;

    // MEM -> WB Latch (Memory to Write Back)
    // Holds the final result to be written back to the register file.
    struct MEM_WB_Latch {
        uint32_t result{0};    // Final data (from ALU or Memory)
        uint8_t rd{0};         // Destination Register
        bool reg_write{false}; // Control signal: Write to register?
        bool valid{false};
    } mem_wb_reg, mem_wb_next;

    // =========================================================================
    // Control & State
    // =========================================================================
    uint32_t pc_register{0};       // Current Program Counter
    bool stall_fetch{false};       // Stall Signal: Stop fetching new instructions
    bool flush_pipeline{false};    // Flush Signal: Clear pipeline stages (e.g., on misprediction)
    uint32_t pc_redirect_target{0};// Target address for redirect (Branch/Jump)
    bool pc_redirect_valid{false}; // Flag indicating valid redirect

    // Scoreboard for hazard detection
    // Tracks which registers are currently pending a write from an instruction in the pipeline.
    // true = register is busy (being written to), false = register is ready.
    bool scoreboard[32]{false};

    // Statistics for cycle-accurate model
    struct Stats {
        uint64_t cycles{0};
        uint64_t instructions{0};
        double get_cpi() const { return instructions > 0 ? (double)cycles / instructions : 0; }
    };
    Stats stats;

    // =========================================================================
    // Stage Methods
    // =========================================================================
    void pc_select();
    void IF_stage();
    void ID_stage();
    void IS_stage();
    void EX_stage();
    void MEM_stage();
    void WB_stage();

    void cycle_thread();

    // =========================================================================
    // Helpers
    // =========================================================================
    bool fetch_instruction(uint32_t addr, uint32_t& data);
    // DMI uses base class members: dmi_ptr_valid, dmi_ptr (inherited from CPU)
    // Only need to track the address range locally
    sc_dt::uint64 dmi_start_addr{0};
    sc_dt::uint64 dmi_end_addr{0};
};

} // namespace riscv_tlm

#endif // CPU_P32_6_CYCLE_H
