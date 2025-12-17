/*!
 \file VPTop.cpp
 \brief Virtual Prototype top-level that assembles CPU, Bus, Memory, Timer, Trace and stub peripherals
 */
// SPDX-License-Identifier: GPL-3.0-or-later

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "VPTop.h"
#if defined(ENABLE_PIPELINED_ISS)
#include "CPU_P32_2.h"
#include "CPU_P64_2.h"
#endif
#ifndef _WIN32
#include "Debug.h"
#endif

namespace vp {

VPTop::VPTop(sc_core::sc_module_name const &name,
             const std::string &hex_file,
             riscv_tlm::cpu_types_t cpu_type,
             bool debug_mode)
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
      clk("clk", sc_core::sc_time(10, sc_core::SC_NS))
{

    std::uint32_t start_PC;

    MainMemory = new riscv_tlm::Memory("Main_Memory", hex_file);
    start_PC = MainMemory->getPCfromHEX();

    // Create 2-stage pipelined CPU based on architecture
    if (m_cpu_type == riscv_tlm::RV32) {
    #if defined(ENABLE_PIPELINED_ISS)
        cpu = new riscv_tlm::CPURV32P2("cpu", start_PC, m_debug);
    #else
        std::cerr << "Error: Pipelined ISS not enabled at compile time." << std::endl;
        std::exit(1);
    #endif
    } else {
    #if defined(ENABLE_PIPELINED_ISS)
        cpu = new riscv_tlm::CPURV64P2("cpu", start_PC, m_debug);
    #else
        std::cerr << "Error: Pipelined ISS not enabled at compile time." << std::endl;
        std::exit(1);
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
    Bus->uart_socket.bind(uart->socket);
    Bus->clint_socket.bind(clint->socket);
    Bus->plic_socket.bind(plic->socket);
    Bus->dma_socket.bind(dma->socket);
    Bus->syscall_socket.bind(sysif->socket);

    dma->mem_master.bind(Bus->dma_master_socket);
    timer->irq_line.bind(cpu->irq_line_socket);

#ifndef _WIN32
    if (m_debug) {
        std::cerr << "Warning: Debug is not supported for pipelined CPUs. Disabling debug." << std::endl;
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