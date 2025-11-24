// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <cstdint>
#include <iostream>
#include <cstring>

namespace riscv_tlm { namespace peripherals {
// Minimal CLINT model exposing mtime/mtimecmp (no MSIP implemented yet)
class CLINT : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<CLINT> socket;

    SC_HAS_PROCESS(CLINT);
    explicit CLINT(sc_core::sc_module_name const &name)
        : sc_module(name), socket("socket"), m_mtime(0), m_mtimecmp(0) {
        socket.register_b_transport(this, &CLINT::b_transport);
        // Simple time progression thread (increments every microsecond of sim time)
        SC_THREAD(tick);
    }

private:
    void tick() {
        while (true) {
            wait(sc_core::sc_time(1, sc_core::SC_US));
            ++m_mtime;
        }
    }

    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        (void)delay;
        auto cmd = trans.get_command();
        uint64_t addr = trans.get_address();
        unsigned char *ptr = trans.get_data_ptr();
        unsigned len = trans.get_data_length();
        // Offsets (RV privileged spec) — using only 64-bit mtimecmp/mtime
        // 0x4000: mtimecmp (low 32) 0x4004: high 32  (we accept 8B)
        // 0xBFF8: mtime     (low 32) 0xBFFC: high 32
        if (len == 8) {
            if (cmd == tlm::TLM_WRITE_COMMAND) {
                uint64_t value = 0; std::memcpy(&value, ptr, 8);
                if (addr == 0x4000) { m_mtimecmp = value; }
                else if (addr == 0xBFF8) { m_mtime = value; }
            } else if (cmd == tlm::TLM_READ_COMMAND) {
                uint64_t value = 0;
                if (addr == 0x4000) value = m_mtimecmp;
                else if (addr == 0xBFF8) value = m_mtime;
                std::memcpy(ptr, &value, 8);
            }
        } else if (len == 4) { // allow 32-bit accesses
            uint32_t value32 = 0;
            if (cmd == tlm::TLM_WRITE_COMMAND) {
                std::memcpy(&value32, ptr, 4);
                if (addr == 0x4000) { m_mtimecmp = (m_mtimecmp & 0xFFFFFFFF00000000ULL) | value32; }
                else if (addr == 0x4004) { m_mtimecmp = (m_mtimecmp & 0xFFFFFFFFULL) | (uint64_t(value32) << 32); }
                else if (addr == 0xBFF8) { m_mtime = (m_mtime & 0xFFFFFFFF00000000ULL) | value32; }
                else if (addr == 0xBFFC) { m_mtime = (m_mtime & 0xFFFFFFFFULL) | (uint64_t(value32) << 32); }
            } else if (cmd == tlm::TLM_READ_COMMAND) {
                if (addr == 0x4000) value32 = uint32_t(m_mtimecmp & 0xFFFFFFFFULL);
                else if (addr == 0x4004) value32 = uint32_t(m_mtimecmp >> 32);
                else if (addr == 0xBFF8) value32 = uint32_t(m_mtime & 0xFFFFFFFFULL);
                else if (addr == 0xBFFC) value32 = uint32_t(m_mtime >> 32);
                std::memcpy(ptr, &value32, 4);
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    uint64_t m_mtime;
    uint64_t m_mtimecmp;
};
}} // namespace
