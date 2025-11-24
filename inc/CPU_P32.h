// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef CPU_P32_H
#define CPU_P32_H

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

class CPURV32P : public CPU {
public:
    using BaseType = std::uint32_t;

    CPURV32P(sc_core::sc_module_name const& name, BaseType PC, bool debug);
    ~CPURV32P() override;

    void set_clock(sc_core::sc_clock* c) override { clk = c; }

    // Execute one pipeline cycle
    bool CPU_step() override;

    // IRQ plumbing similar to RV32
    bool cpu_process_IRQ() override;
    void call_interrupt(tlm::tlm_generic_payload &m_trans, sc_core::sc_time &delay) override;

    std::uint64_t getStartDumpAddress() override;
    std::uint64_t getEndDumpAddress() override;

    bool isPipelined() const override { return true; }

private:
    // Minimal single-entry pipeline record (fetch + execute in next cycle)
    struct IFID {
        BaseType pc{0};
        std::uint32_t instr{0};
        bool valid{false};
    };

    // Architectural state and helpers
    Registers<BaseType>*     register_bank{nullptr};
    BASE_ISA<BaseType>*      base_inst{nullptr};
    C_extension<BaseType>*   c_inst{nullptr};
    M_extension<BaseType>*   m_inst{nullptr};
    A_extension<BaseType>*   a_inst{nullptr};

    // Fetch/DMI
    tlm::tlm_generic_payload trans{};
    unsigned char*           dmi_ptr{nullptr};
    bool                     dmi_ptr_valid{false};

    // IRQ bookkeeping
    BaseType int_cause{0};

    // Pipeline state
    IFID if_id{}, if_id_next{};
    sc_core::sc_clock* clk{nullptr};

    // Stages
    void stageIF();
    void stageID() {}
    void stageEX() {}
    void stageMEM() {}
    void stageWB(bool &breakpoint);

    void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) { dmi_ptr_valid = false; }
};

} // namespace riscv_tlm

#endif // CPU_P32_H
