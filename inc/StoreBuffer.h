// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file StoreBuffer.h
 * @brief Store Buffer for holding stores until commit
 * @note Ensures memory consistency by deferring store operations until commit
 */
#pragma once
#ifndef STOREBUFFER_H
#define STOREBUFFER_H

#include <cstdint>
#include <array>

namespace riscv_tlm {

/**
 * @brief Store Buffer Entry
 */
struct StoreBufferEntry {
    bool valid{false};
    uint64_t address{0};
    uint64_t data{0};
    int size{0};            // 1, 2, 4, or 8 bytes
    int rob_index{-1};      // Associated ROB entry
};

/**
 * @brief Store Buffer Class
 * Holds store operations until they are committed
 */
template<std::size_t SIZE = 8>
class StoreBuffer {
public:
    StoreBuffer() = default;

    /**
     * @brief Add a store to the buffer
     * @return Index in store buffer, or -1 if full
     */
    int add_store(uint64_t address, uint64_t data, int size, int rob_index) {
        // Find free entry
        for (std::size_t i = 0; i < SIZE; i++) {
            if (!entries[i].valid) {
                entries[i].valid = true;
                entries[i].address = address;
                entries[i].data = data;
                entries[i].size = size;
                entries[i].rob_index = rob_index;
                return i;
            }
        }
        return -1; // Full
    }

    /**
     * @brief Commit a store (given ROB index)
     * Returns the store data so caller can write to memory
     */
    bool commit_store(int rob_index, uint64_t& address, uint64_t& data, int& size) {
        for (std::size_t i = 0; i < SIZE; i++) {
            if (entries[i].valid && entries[i].rob_index == rob_index) {
                address = entries[i].address;
                data = entries[i].data;
                size = entries[i].size;
                entries[i].valid = false; // Deallocate
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Flush all stores (e.g., on exception)
     */
    void flush() {
        for (auto& entry : entries) {
            entry.valid = false;
        }
    }

    /**
     * @brief Check if store buffer is full
     */
    bool is_full() const {
        for (const auto& entry : entries) {
            if (!entry.valid) return false;
        }
        return true;
    }

private:
    std::array<StoreBufferEntry, SIZE> entries;
};

} // namespace riscv_tlm

#endif // STOREBUFFER_H
