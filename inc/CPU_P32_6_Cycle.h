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
#include "A_extension.h"
#include "Performance.h"

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
    A_extension<BaseType>*  a_inst{nullptr};

    BaseType int_cause{0};
    
    sc_core::sc_clock* clk{nullptr};
    sc_core::sc_time clock_period{10, sc_core::SC_NS};

    // =========================================================================
    // Pipeline Latches
    // =========================================================================
    
    // IF -> ID
    struct IF_ID_Latch {
        uint32_t pc{0};
        uint32_t instr{0};
        bool valid{false};
    } if_id_reg, if_id_next;

    // ID -> IS
    struct ID_IS_Latch {
        uint32_t pc{0};
        uint32_t instr{0};
        uint8_t rd{0}, rs1{0}, rs2{0};
        int32_t imm{0};
        uint8_t opcode{0};
        uint8_t funct3{0};
        uint8_t funct7{0};
        bool valid{false};
    } id_is_reg, id_is_next;

    // IS -> EX
    struct IS_EX_Latch {
        uint32_t pc{0};
        uint32_t rs1_val{0};
        uint32_t rs2_val{0};
        int32_t imm{0};
        uint8_t rd{0};
        uint8_t opcode{0};
        uint8_t funct3{0};
        uint8_t funct7{0};
        bool valid{false};
    } is_ex_reg, is_ex_next;

    // EX -> MEM
    struct EX_MEM_Latch {
        uint32_t pc{0};
        uint32_t alu_result{0};
        uint32_t store_data{0};
        uint8_t rd{0};
        uint8_t funct3{0};
        bool mem_read{false};
        bool mem_write{false};
        bool branch_taken{false};
        uint32_t branch_target{0};
        bool valid{false};
    } ex_mem_reg, ex_mem_next;

    // MEM -> WB
    struct MEM_WB_Latch {
        uint32_t result{0};
        uint8_t rd{0};
        bool reg_write{false};
        bool valid{false};
    } mem_wb_reg, mem_wb_next;

    // =========================================================================
    // Control & State
    // =========================================================================
    uint32_t pc_register{0};
    bool stall_fetch{false};
    bool flush_pipeline{false};
    uint32_t pc_redirect_target{0};
    bool pc_redirect_valid{false};

    // Scoreboard for hazard detection
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
