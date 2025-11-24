// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef CPU_P64_H
#define CPU_P64_H

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

class CPURV64P : public CPU {
public:
    using BaseType = std::uint64_t;

    CPURV64P(sc_core::sc_module_name const& name, BaseType PC, bool debug);
    ~CPURV64P() override;

    void set_clock(sc_core::sc_clock* c) override { clk = c; }

    bool CPU_step() override;

    bool cpu_process_IRQ() override;
    void call_interrupt(tlm::tlm_generic_payload &m_trans, sc_core::sc_time &delay) override;

    std::uint64_t getStartDumpAddress() override;
    std::uint64_t getEndDumpAddress() override;

private:
    struct IFID {
        BaseType pc{0};
        std::uint32_t instr{0};
        bool valid{false};
    };

    Registers<BaseType>*     register_bank{nullptr};
    BASE_ISA<BaseType>*      base_inst{nullptr};
    C_extension<BaseType>*   c_inst{nullptr};
    M_extension<BaseType>*   m_inst{nullptr};
    A_extension<BaseType>*   a_inst{nullptr};

    tlm::tlm_generic_payload trans{};
    unsigned char*           dmi_ptr{nullptr};
    bool                     dmi_ptr_valid{false};

    BaseType int_cause{0};

    IFID if_id{}, if_id_next{};
    sc_core::sc_clock* clk{nullptr};

    void stageIF();
    void stageID() {}
    void stageEX() {}
    void stageMEM() {}
    void stageWB(bool &breakpoint);

    void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) { dmi_ptr_valid = false; }
};

} // namespace riscv_tlm

#endif // CPU_P64_H
