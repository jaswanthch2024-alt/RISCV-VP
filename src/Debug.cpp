/*!
 \file Debug.cpp
 \brief GDB connector
 \author Màrius Montón
 \date February 2021
 */
// SPDX-License-Identifier: GPL-3.0-or-later


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "Debug.h"

namespace riscv_tlm {
    constexpr char nibble_to_hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

    // Simple endian swap for 32-bit (matches htonl for little-endian hosts)
    static inline std::uint32_t host_to_be32(std::uint32_t v) {
        return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
    }

    Debug::Debug(riscv_tlm::CPURV32 *cpu, Memory *mem) : sc_module(sc_core::sc_module_name("Debug")) {
        dbg_cpu32 = cpu; dbg_cpu64 = nullptr; register_bank32 = nullptr; register_bank64 = nullptr; dbg_mem = mem; cpu_type = riscv_tlm::RV32; conn = -1; std::cout << "[Debug] GDB remote stub disabled on Windows." << std::endl; }
    Debug::Debug(riscv_tlm::CPURV64 *cpu, Memory *mem) : sc_module(sc_core::sc_module_name("Debug")) {
        dbg_cpu32 = nullptr; dbg_cpu64 = cpu; register_bank32 = nullptr; register_bank64 = nullptr; dbg_mem = mem; cpu_type = riscv_tlm::RV64; conn = -1; std::cout << "[Debug] GDB remote stub disabled on Windows." << std::endl; }
    Debug::~Debug() = default;
    void Debug::send_packet(int, const std::string &) { /* no-op */ }
    std::string Debug::receive_packet() { return ""; }
    void Debug::handle_gdb_loop() { /* no-op */ }
    std::string Debug::compute_checksum_string(const std::string &msg) {
        unsigned sum = 0; for(char c: msg) sum += unsigned(c); sum &= 0xFFu; char low = nibble_to_hex[sum & 0xF]; char high = nibble_to_hex[(sum >> 4) & 0xF]; return {high, low}; }
}