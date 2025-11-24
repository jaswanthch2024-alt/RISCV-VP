// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <cstdint>
#include <iostream>

namespace riscv_tlm { namespace peripherals {

class UART : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<UART> socket;

    SC_HAS_PROCESS(UART);
    explicit UART(sc_core::sc_module_name const& name): sc_module(name), socket("socket") {
        socket.register_b_transport(this, &UART::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        (void)delay;
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND && len > 0) {
            // Simple: write first byte to stdout
            char c = static_cast<char>(ptr[0]);
            std::cout << c << std::flush;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

}} // namespace
