/*!
 \file VPMain.cpp
 \brief Virtual Prototype entry point (sc_main) with CLI and stats
 */
// SPDX-License-Identifier: GPL-3.0-or-later

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "systemc"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <cmath>
#include <iomanip>
#include <cstdlib>

#include "VPTop.h"
#include "Performance.h"

// Add spdlog includes to ensure a default logger is available
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/null_sink.h"  // Added for fallback

static vp::VPTop *g_top = nullptr;

static void intHandler(int dummy) {
    if (g_top) {
        delete g_top;
        g_top = nullptr;
    }
    (void)dummy;
    // Use sc_stop() if possible, otherwise exit cleanly
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) {
        sc_core::sc_stop();
    }
    std::exit(0);  // Changed to exit(0) for clean exit
}

struct Options {
    std::string hex_file;
    bool debug = false;
    riscv_tlm::cpu_types_t cpu_type = riscv_tlm::RV32;
    double timeout_sec = -1.0; // <=0 means run until program stops
    std::uint64_t max_instructions = 0; // 0 means no instruction cap
    bool pipelined_rv32 =
    #if defined(ENABLE_PIPELINED_ISS)
        true;
    #else
        false;
    #endif
};

static void usage(const char* exe) {
    std::cout << "Usage: " << exe << " -f <file.hex> [-R 32|64] [-D] [-t <seconds>] [--max-instr <N>] [--rv32-pipe on|off]\n";
}

static Options parse(int argc, char* argv[]) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "-f") == 0 || std::strcmp(argv[i], "--file") == 0) && i+1 < argc) {
            o.hex_file = argv[++i];
        } else if (std::strcmp(argv[i], "-D") == 0 || std::strcmp(argv[i], "--debug") == 0) {
            o.debug = true;
        } else if ((std::strcmp(argv[i], "-R") == 0 || std::strcmp(argv[i], "--arch") == 0) && i+1 < argc) {
            o.cpu_type = (std::strcmp(argv[i+1], "64") == 0) ? riscv_tlm::RV64 : riscv_tlm::RV32;
            ++i;
        } else if ((std::strcmp(argv[i], "-t") == 0 || std::strcmp(argv[i], "--timeout") == 0) && i+1 < argc) {
            try {
                o.timeout_sec = std::stod(argv[++i]);
            } catch (...) {
                usage(argv[0]);
                std::exit(1);
            }
        } else if ((std::strcmp(argv[i], "--max-instr") == 0) && i+1 < argc) {
            char* endp = nullptr;
            auto val = std::strtoull(argv[++i], &endp, 10);
            if (endp == nullptr || *endp != '\0') {
                usage(argv[0]);
                std::exit(1);
            }
            o.max_instructions = val;
        } else if ((std::strcmp(argv[i], "--rv32-pipe") == 0) && i+1 < argc) {
            const char* v = argv[++i];
            if (std::strcmp(v, "on") == 0 || std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0) {
                o.pipelined_rv32 = true;
            } else if (std::strcmp(v, "off") == 0 || std::strcmp(v, "0") == 0 || std::strcmp(v, "false") == 0) {
                o.pipelined_rv32 = false;
            } else {
                usage(argv[0]);
                std::exit(1);
            }
        } else {
            usage(argv[0]);
            std::exit(1);
        }
    }
    if (o.hex_file.empty()) {
        usage(argv[0]);
        std::exit(1);
    }
    return o;
}

int sc_main(int argc, char* argv[]) {
    signal(SIGINT, intHandler);
    sc_core::sc_set_time_resolution(1, sc_core::SC_NS);

    const auto opts = parse(argc, argv);

    // Ensure a default logger exists - improved error handling
    try {
        auto existing = spdlog::get("my_logger");
        if (!existing) {
            spdlog::filename_t log_filename = SPDLOG_FILENAME_T("vp.log");
            auto logger = spdlog::create<spdlog::sinks::basic_file_sink_mt>("my_logger", log_filename, true);
            logger->set_pattern("%v");
            logger->set_level(spdlog::level::info);
        }
    } catch (const std::exception& e) {
        // Create null logger as fallback to prevent crashes
        std::cerr << "Warning: Could not setup file logger (" << e.what() << "), using null logger\n";
        auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("my_logger", null_sink);
        spdlog::register_logger(logger);
    }

    auto perf = Performance::getInstance();

    std::cout << "RISC-V VP starting\n";
    std::cout << "  file: " << opts.hex_file << "\n";
    std::cout << "  arch: " << (opts.cpu_type == riscv_tlm::RV32 ? "RV32" : "RV64") << "\n";
    std::cout << "  dbg : " << (opts.debug ? "on" : "off") << "\n";
    if (opts.timeout_sec > 0) {
        std::cout << "  tmo : " << opts.timeout_sec << " s\n";
    }
    if (opts.max_instructions > 0) {
        std::cout << "  max : " << opts.max_instructions << " instr\n";
    }
    if (opts.cpu_type == riscv_tlm::RV32) {
        std::cout << "  pipe: " << (opts.pipelined_rv32 ? "on" : "off") << "\n";
    }

    g_top = new vp::VPTop("vp_top", opts.hex_file, opts.cpu_type, opts.debug, opts.pipelined_rv32);

    auto wall_start = std::chrono::steady_clock::now();

    // Run simulation in chunks to allow checking progress/limits
    const sc_core::sc_time quantum(1, sc_core::SC_MS);
    bool timed_out = false;
    bool reached_instr_limit = false;

    while (true) {
        sc_core::sc_start(quantum);

        // Check timeout condition
        if (opts.timeout_sec > 0) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> wall_elapsed = now - wall_start;
            if (wall_elapsed.count() >= opts.timeout_sec) {
                timed_out = true;
                sc_core::sc_stop();
                break;  // Added explicit break for clarity
            }
        }
        
        // Check instruction limit
        if (opts.max_instructions > 0 && perf->getInstructions() >= opts.max_instructions) {
            reached_instr_limit = true;
            sc_core::sc_stop();
            break;  // Added explicit break for clarity
        }
        
        // Stop if the kernel reports stopped (e.g., via tohost or exceptions)
        if (sc_core::sc_get_status() == sc_core::SC_STOPPED) {
            break;
        }
    }

    auto wall_end = std::chrono::steady_clock::now();

    std::chrono::duration<double> elapsed = wall_end - wall_start;
    double ips = elapsed.count() > 0.0 ? static_cast<double>(perf->getInstructions()) / elapsed.count() : 0.0;

    if (timed_out) {
        std::cout << "Stopped due to timeout." << std::endl;
    }
    if (reached_instr_limit) {
        std::cout << "Stopped after reaching instruction limit." << std::endl;
    }

    std::cout << "Total elapsed time: " << elapsed.count() << "s\n";
    // Removed instr/sec to focus on IPC
    const sc_core::sc_time clk_period(10, sc_core::SC_NS);
    sc_core::sc_time sim_time = sc_core::sc_time_stamp();
    double cycles = sim_time / clk_period;
    double ipc = cycles > 0.0 ? static_cast<double>(perf->getInstructions()) / cycles : 0.0;
    std::cout << "Cycles: " << static_cast<unsigned long long>(cycles)
              << "\nInstr/Cycle: " << std::fixed << std::setprecision(3) << ipc << "\n";

    delete g_top;
    g_top = nullptr;

    return 0;
}