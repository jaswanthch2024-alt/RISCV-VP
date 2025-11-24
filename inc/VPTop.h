/*!
 \file VPTop.h
 \brief Virtual Prototype top-level that assembles CPU, Bus, Memory, Timer, Trace (extended)
 */
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef __VP_TOP_H__
#define __VP_TOP_H__

#pragma once
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "systemc"
#include <memory>
#include <string>

#include "CPU.h"
#include "Memory.h"
#include "BusCtrl.h"
#include "Trace.h"
#include "Timer.h"
#include "UART.h"
#include "CLINT.h"
#include "PLIC.h"
#include "DMA.h"
#include "SyscallIf.h"
#if defined(ENABLE_PIPELINED_ISS)
#include "CPU_P32.h"
#include "CPU_P64.h"
#endif

namespace riscv_tlm { class Debug; }

namespace vp {

class VPTop : public sc_core::sc_module {
public:
    riscv_tlm::CPU *cpu;
    riscv_tlm::Memory *MainMemory;
    riscv_tlm::BusCtrl *Bus;
    riscv_tlm::peripherals::Trace *trace;
    riscv_tlm::peripherals::Timer *timer;
    riscv_tlm::peripherals::UART *uart;
    riscv_tlm::peripherals::CLINT *clint;      // new stub
    riscv_tlm::peripherals::PLIC *plic;        // new stub
    riscv_tlm::peripherals::DMA *dma;          // new stub
    riscv_tlm::peripherals::SyscallIf *sysif;  // new stub

    SC_HAS_PROCESS(VPTop);

    VPTop(sc_core::sc_module_name const &name,
          const std::string &hex_file,
          riscv_tlm::cpu_types_t cpu_type,
          bool debug_mode,
          bool pipelined_rv32 =
#if defined(ENABLE_PIPELINED_ISS)
          true
#else
          false
#endif
          );

    ~VPTop() override;

private:
    bool m_debug;
    riscv_tlm::cpu_types_t m_cpu_type;
    bool m_pipelined; // runtime choice for RV32
#ifndef _WIN32
    std::unique_ptr<riscv_tlm::Debug> m_debugger;
#endif
    sc_core::sc_clock clk; // ADD: clock lives as long as the top
};

} // namespace vp
#endif