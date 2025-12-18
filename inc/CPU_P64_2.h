// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef CPU_P64_2_H
#define CPU_P64_2_H

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include "systemc"
#include "tlm.h"

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

/**
 * @brief 2-Stage Pipelined RISC-V 64-bit CPU with AT timing (for VP)
 * 
 * Pipeline stages:
 *   IF  -> EX
 *   Fetch  Decode+Execute+Memory+Writeback
 * 
 * This is an Approximately-Timed (AT) model that:
 * - Waits for actual clock cycles
 * - Models branch flush penalties
 * - Provides cycle-accurate statistics
 * 
 * For fast functional simulation, use CPURV64Simple instead.
 */
class CPURV64P2 : public CPU {
public:
    using BaseType = std::uint64_t;

    CPURV64P2(sc_core::sc_module_name const& name, BaseType PC, bool debug);
    ~CPURV64P2() override;

    void set_clock(sc_core::sc_clock* c) override { clk = c; }

    bool CPU_step() override;

    bool cpu_process_IRQ() override;
    void call_interrupt(tlm::tlm_generic_payload &m_trans, sc_core::sc_time &delay) override;

    std::uint64_t getStartDumpAddress() override;
    std::uint64_t getEndDumpAddress() override;

    bool isPipelined() const override { return true; }

    // Pipeline statistics
    struct PipelineStats {
        uint64_t cycles{0};
        uint64_t stalls{0};
        uint64_t flushes{0};
        uint64_t control_hazards{0};
    };
    
    PipelineStats getStats() const { return stats; }

private:
    // =========================================================================
    // Architectural State
    // =========================================================================
    Registers<BaseType>*     register_bank{nullptr};
    BASE_ISA<BaseType>*      base_inst{nullptr};
    C_extension<BaseType>*   c_inst{nullptr};
    M_extension<BaseType>*   m_inst{nullptr};
    A_extension<BaseType>*   a_inst{nullptr};

    // Instruction fetch
    std::uint32_t INSTR{0};

    // IRQ bookkeeping
    BaseType int_cause{0};

    sc_core::sc_clock* clk{nullptr};

    // Statistics
    PipelineStats stats{};

    // Clock period for AT timing
    sc_core::sc_time clock_period{10, sc_core::SC_NS};

    void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) { dmi_ptr_valid = false; }
};

} // namespace riscv_tlm

#endif // CPU_P64_2_H
