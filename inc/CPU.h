/**
 \file CPU.h
 \brief Main CPU base class
 \author Màrius Montón
 \date August 2018
 */
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CPU_BASE_H
#define CPU_BASE_H

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

#include "BASE_ISA.h"
#include "C_extension.h"
#include "M_extension.h"
#include "A_extension.h"
#include "MemoryInterface.h"
#include "Performance.h"
#include "Registers.h"

namespace riscv_tlm {

    typedef enum {RV32, RV64} cpu_types_t;

    /**
     * @brief Abstract base class for RISC-V CPU models
     * 
     * All CPU implementations (pipelined or otherwise) derive from this class.
     */
    class CPU : sc_core::sc_module  {
    public:
        virtual void set_clock(sc_core::sc_clock* c) { (void)c; }
        
        // Identify pipelined cores
        virtual bool isPipelined() const { return false; }

        /* Constructors */
        explicit CPU(sc_core::sc_module_name const &name, bool debug);

        CPU() noexcept = delete;
        CPU(const CPU& other) noexcept = delete;
        CPU(CPU && other) noexcept = delete;
        CPU& operator=(const CPU& other) noexcept = delete;
        CPU& operator=(CPU&& other) noexcept = delete;

        /* Destructors */
        ~CPU() override = default;

        /**
         * @brief Perform one instruction step
         * @return Breakpoint found
         */
        virtual bool CPU_step() = 0;

        /**
         * @brief Instruction Memory bus socket
         */
        tlm_utils::simple_initiator_socket<CPU> instr_bus;

        /**
         * @brief IRQ line socket
         */
        tlm_utils::simple_target_socket<CPU> irq_line_socket;

        /**
        * @brief DMI pointer is not longer valid
        * @param start memory address region start
        * @param end memory address region end
        */
        void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end);

        /**
        * @brief CPU main thread
        */
        [[noreturn]] void CPU_thread();

        /**
         * @brief Process and triggers IRQ if all conditions met
         * @return true if IRQ is triggered, false otherwise
         */
        virtual bool cpu_process_IRQ() = 0;

        /**
         * @brief callback for IRQ simple socket
         * @param trans transaction to perform
         * @param delay time to annotate
         */
        virtual void call_interrupt(tlm::tlm_generic_payload &trans,
                            sc_core::sc_time &delay) = 0;

        virtual std::uint64_t getStartDumpAddress() = 0;
        virtual std::uint64_t getEndDumpAddress() = 0;

        /**
         * @brief AT protocol backward path callback
         * 
         * Virtual function for AT model support. Default does nothing (LT models).
         * AT model CPUs override this to handle non-blocking responses.
         * 
         * @param trans TLM transaction
         * @param phase Current TLM phase
         * @param delay Annotated delay
         * @return TLM sync status
         */
        virtual tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload &trans,
                                                    tlm::tlm_phase &phase,
                                                    sc_core::sc_time &delay);

    public:
        MemoryInterface *mem_intf;
        
    protected:
        Performance *perf;
        std::shared_ptr<spdlog::logger> logger;
        tlm_utils::tlm_quantumkeeper *m_qk;
        Instruction inst;
        bool interrupt;
        bool irq_already_down;
        sc_core::sc_time default_time;
        bool dmi_ptr_valid;
        tlm::tlm_generic_payload trans;
        unsigned char *dmi_ptr = nullptr;
        bool last_mem_access = false;
    };

} // namespace riscv_tlm

#endif // CPU_BASE_H
