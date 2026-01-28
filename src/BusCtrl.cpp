/**
 @file BusCtrl.cpp
 @brief Basic TLM-2 Bus controller (extended)
 */
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BusCtrl.h"

namespace riscv_tlm {

    SC_HAS_PROCESS(BusCtrl);

    BusCtrl::BusCtrl(sc_core::sc_module_name const &name) :
            sc_module(name),
            cpu_instr_socket("cpu_instr_socket"),
            cpu_data_socket("cpu_data_socket"),
            dma_master_socket("dma_master_socket"),
            memory_socket("memory_socket"),
            trace_socket("trace_socket"),
            timer_socket("timer_socket"),
            uart_socket("uart_socket"),
            clint_socket("clint_socket"),
            plic_socket("plic_socket"),
            dma_socket("dma_socket"),
            syscall_socket("syscall_socket") {

        // All masters enter through the same b_transport
        cpu_instr_socket.register_b_transport(this, &BusCtrl::b_transport);
        cpu_data_socket.register_b_transport(this, &BusCtrl::b_transport);
        dma_master_socket.register_b_transport(this, &BusCtrl::b_transport);

        cpu_instr_socket.register_get_direct_mem_ptr(this,
                                                     &BusCtrl::instr_direct_mem_ptr);
        memory_socket.register_invalidate_direct_mem_ptr(this,
                                                         &BusCtrl::invalidate_direct_mem_ptr);
    }

    void BusCtrl::b_transport(tlm::tlm_generic_payload &trans,
                              sc_core::sc_time &delay) {

        sc_dt::uint64 adr_bytes = trans.get_address();
        sc_dt::uint64 adr = adr_bytes / 4;

        // Specific check for legacy TO_HOST (0x90000000)
        // Check EXACT match avoid trapping high memory usage (stack)
        if (adr == TO_HOST_ADDRESS / 4) {
            std::cout << "To host (legacy)\n" << std::flush;
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            sc_core::sc_stop();
            return;
        }
        
        // Trap standard Spike/RISC-V-Tests tohost address (0x80001000)
        // adr is dword index (byte_addr / 4)
        // 0x80001000 / 4 = 0x20000400
        if (adr == 0x20000400) { 
            if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
                 uint32_t val = 0;
                 if (trans.get_data_length() >= 4) {
                     memcpy(&val, trans.get_data_ptr(), 4);
                 }
                 if (val != 0) { // Only stop if non-zero is written (return code)
                     std::cout << "To host (0x80001000) detected. termination code: " << val << "\n" << std::flush;
                     sc_core::sc_stop();
                     return;
                 }
            }
        }

        // Decode by region (simple range checks). Optional targets are checked for binding.
        if (adr_bytes >= UART0_BASE_ADDRESS && adr_bytes < UART0_BASE_ADDRESS + 0x100) {
            if (uart_socket.size() > 0) {
                uart_socket->b_transport(trans, delay);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }
        if (adr_bytes >= CLINT_BASE_ADDRESS && adr_bytes < CLINT_BASE_ADDRESS + 0x10000) {
            if (clint_socket.size() > 0) {
                clint_socket->b_transport(trans, delay);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }
        if (adr_bytes >= PLIC_BASE_ADDRESS && adr_bytes < PLIC_BASE_ADDRESS + 0x400000) {
            if (plic_socket.size() > 0) {
                plic_socket->b_transport(trans, delay);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }
        if (adr_bytes >= DMA_BASE_ADDRESS && adr_bytes < DMA_BASE_ADDRESS + 0x1000) {
            if (dma_socket.size() > 0) {
                dma_socket->b_transport(trans, delay);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }
        if (adr_bytes >= SYSCALL_BASE_ADDRESS && adr_bytes < SYSCALL_BASE_ADDRESS + 0x1000) {
            if (syscall_socket.size() > 0) {
                syscall_socket->b_transport(trans, delay);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }

        switch (adr) {
            case TIMER_MEMORY_ADDRESS_HI / 4:
            case TIMER_MEMORY_ADDRESS_LO / 4:
            case TIMERCMP_MEMORY_ADDRESS_HI / 4:
            case TIMERCMP_MEMORY_ADDRESS_LO / 4:
                timer_socket->b_transport(trans, delay);
                break;
            case TRACE_MEMORY_ADDRESS / 4:
                trace_socket->b_transport(trans, delay);
                break;
            default:
                memory_socket->b_transport(trans, delay);
                break;
        }

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    bool BusCtrl::instr_direct_mem_ptr(tlm::tlm_generic_payload &gp,
                                       tlm::tlm_dmi &dmi_data) {
        return memory_socket->get_direct_mem_ptr(gp, dmi_data);
    }

    void BusCtrl::invalidate_direct_mem_ptr(sc_dt::uint64 start,
                                            sc_dt::uint64 end) {
        cpu_instr_socket->invalidate_direct_mem_ptr(start, end);
    }
}