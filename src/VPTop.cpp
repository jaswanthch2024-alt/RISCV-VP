/*!
 \file VPTop.cpp
 \brief Virtual Prototype top-level that assembles CPU, Bus, Memory, Timer, Trace and stub peripherals
 */
// SPDX-License-Identifier: GPL-3.0-or-later

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "VPTop.h"
#if defined(ENABLE_PIPELINED_ISS)
#include "CPU_P32.h"
#endif
#ifndef _WIN32
#include "Debug.h" // Needed for constructing Debug in this TU
#endif

namespace vp {

VPTop::VPTop(sc_core::sc_module_name const &name,
             const std::string &hex_file,
             riscv_tlm::cpu_types_t cpu_type,
             bool debug_mode,
             bool pipelined_rv32)
    : sc_module(name),
      cpu(nullptr),
      MainMemory(nullptr),
      Bus(nullptr),
      trace(nullptr),
      timer(nullptr),
      uart(nullptr),
      clint(nullptr),
      plic(nullptr),
      dma(nullptr),
      sysif(nullptr),
      m_debug(debug_mode),
      m_cpu_type(cpu_type),
      m_pipelined(pipelined_rv32),
      clk("clk", sc_core::sc_time(10, sc_core::SC_NS)) // ADD: initialize clock
{

    std::uint32_t start_PC;

    MainMemory = new riscv_tlm::Memory("Main_Memory", hex_file);
    start_PC = MainMemory->getPCfromHEX();

    if (m_cpu_type == riscv_tlm::RV32) {
    #if defined(ENABLE_PIPELINED_ISS)
        if (m_pipelined) {
            cpu = new riscv_tlm::CPURV32P("cpu", start_PC, m_debug);
        } else {
            cpu = new riscv_tlm::CPURV32("cpu", start_PC, m_debug);
        }
    #else
        cpu = new riscv_tlm::CPURV32("cpu", start_PC, m_debug);
    #endif
    } else {
    #if defined(ENABLE_PIPELINED_ISS)
        if (m_pipelined) {
            cpu = new riscv_tlm::CPURV64P("cpu", start_PC, m_debug);
        } else {
            cpu = new riscv_tlm::CPURV64("cpu", start_PC, m_debug);
        }
    #else
        cpu = new riscv_tlm::CPURV64("cpu", start_PC, m_debug);
    #endif
    }

    cpu->set_clock(&clk);

    Bus   = new riscv_tlm::BusCtrl("BusCtrl");
    trace = new riscv_tlm::peripherals::Trace("Trace");
    timer = new riscv_tlm::peripherals::Timer("Timer");
    uart  = new riscv_tlm::peripherals::UART("UART0");
    clint = new riscv_tlm::peripherals::CLINT("CLINT");
    plic  = new riscv_tlm::peripherals::PLIC("PLIC");
    dma   = new riscv_tlm::peripherals::DMA("DMA");
    sysif = new riscv_tlm::peripherals::SyscallIf("SysIf");

    cpu->instr_bus.bind(Bus->cpu_instr_socket);
    cpu->mem_intf->data_bus.bind(Bus->cpu_data_socket);

    Bus->memory_socket.bind(MainMemory->socket);
    Bus->trace_socket.bind(trace->socket);
    Bus->timer_socket.bind(timer->socket);

    // Optional peripherals: bind if present
    Bus->uart_socket.bind(uart->socket);
    Bus->clint_socket.bind(clint->socket);
    Bus->plic_socket.bind(plic->socket);
    Bus->dma_socket.bind(dma->socket);           // register interface
    Bus->syscall_socket.bind(sysif->socket);

    // Allow DMA to access system memory through the bus decoder rather than direct binding.
    // Connect DMA master to the Bus's dedicated target socket.
    dma->mem_master.bind(Bus->dma_master_socket);

    timer->irq_line.bind(cpu->irq_line_socket); // existing timer IRQ only

#ifndef _WIN32
    if (m_debug) {
        if (m_cpu_type == riscv_tlm::RV32) {
        #if defined(ENABLE_PIPELINED_ISS)
            if (!m_pipelined) {
                m_debugger.reset(new riscv_tlm::Debug(dynamic_cast<riscv_tlm::CPURV32*>(cpu), MainMemory));
            } else {
                std::cerr << "Warning: Debug is not supported for pipelined RV32 (CPURV32P). Disabling debug." << std::endl;
            }
        #else
            m_debugger.reset(new riscv_tlm::Debug(dynamic_cast<riscv_tlm::CPURV32*>(cpu), MainMemory));
        #endif
        } else {
            m_debugger.reset(new riscv_tlm::Debug(dynamic_cast<riscv_tlm::CPURV64*>(cpu), MainMemory));
        }
    }
#endif
}

VPTop::~VPTop() {
    delete sysif;
    delete dma;
    delete plic;
    delete clint;
    delete uart;
    delete timer;
    delete trace;
    delete Bus;
    delete cpu;
    delete MainMemory;
}

} // namespace vp