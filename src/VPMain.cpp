/*!
 \file VPMain.cpp
 \brief Virtual Prototype entry point (sc_main) with CLI and stats
 \note Uses Approximately-Timed (AT) 2-stage pipelined CPU
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
#if defined(ENABLE_PIPELINED_ISS)
#include "CPU_P32_2.h"
#include "CPU_P64_2.h"
#endif

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/null_sink.h"

static vp::VPTop *g_top = nullptr;

static void intHandler(int dummy) {
    if (g_top) {
        delete g_top;
        g_top = nullptr;
    }
    (void)dummy;
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) {
        sc_core::sc_stop();
    }
    std::exit(0);
}

struct Options {
    std::string hex_file;
    bool debug = false;
    riscv_tlm::cpu_types_t cpu_type = riscv_tlm::RV32;
    double timeout_sec = -1.0;
    std::uint64_t max_instructions = 0;
};

static void usage(const char* exe) {
    std::cout << "Usage: " << exe << " -f <file.hex> [-R 32|64] [-D] [-t <seconds>] [--max-instr <N>]\n";
    std::cout << "\nRISC-V Virtual Prototype with AT 2-stage pipelined CPU\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -f, --file <file.hex>   Input hex file (required)\n";
    std::cout << "  -R, --arch 32|64        Architecture: RV32 or RV64 (default: 32)\n";
    std::cout << "  -D, --debug             Enable debug mode\n";
    std::cout << "  -t, --timeout <sec>     Wall-clock timeout in seconds\n";
    std::cout << "  --max-instr <N>         Maximum instructions to execute\n";
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
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            std::exit(0);
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

    // Setup logger
    try {
        auto existing = spdlog::get("my_logger");
        if (!existing) {
            spdlog::filename_t log_filename = SPDLOG_FILENAME_T("vp.log");
            auto logger = spdlog::create<spdlog::sinks::basic_file_sink_mt>("my_logger", log_filename, true);
            logger->set_pattern("%v");
            logger->set_level(spdlog::level::info);
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not setup file logger (" << e.what() << "), using null logger\n";
        auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("my_logger", null_sink);
        spdlog::register_logger(logger);
    }

    auto perf = Performance::getInstance();

    std::cout << "RISC-V Virtual Prototype (Approximately-Timed)\n";
    std::cout << "  file: " << opts.hex_file << "\n";
    std::cout << "  arch: " << (opts.cpu_type == riscv_tlm::RV32 ? "RV32" : "RV64") << "\n";
    std::cout << "  mode: AT (cycle-accurate)\n";
    std::cout << "  pipe: 2-stage (IF -> EX)\n";
    std::cout << "  dbg : " << (opts.debug ? "on" : "off") << "\n";
    if (opts.timeout_sec > 0) {
        std::cout << "  tmo : " << opts.timeout_sec << " s\n";
    }
    if (opts.max_instructions > 0) {
        std::cout << "  max : " << opts.max_instructions << " instr\n";
    }

    g_top = new vp::VPTop("vp_top", opts.hex_file, opts.cpu_type, opts.debug);

    auto wall_start = std::chrono::steady_clock::now();

    const sc_core::sc_time quantum(1, sc_core::SC_MS);
    bool timed_out = false;
    bool reached_instr_limit = false;

    while (true) {
        sc_core::sc_start(quantum);

        if (opts.timeout_sec > 0) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> wall_elapsed = now - wall_start;
            if (wall_elapsed.count() >= opts.timeout_sec) {
                timed_out = true;
                sc_core::sc_stop();
                break;
            }
        }
        
        if (opts.max_instructions > 0 && perf->getInstructions() >= opts.max_instructions) {
            reached_instr_limit = true;
            sc_core::sc_stop();
            break;
        }
        
        if (sc_core::sc_get_status() == sc_core::SC_STOPPED) {
            break;
        }
    }

    auto wall_end = std::chrono::steady_clock::now();

    std::chrono::duration<double> elapsed = wall_end - wall_start;

    if (timed_out) {
        std::cout << "Stopped due to timeout." << std::endl;
    }
    if (reached_instr_limit) {
        std::cout << "Stopped after reaching instruction limit." << std::endl;
    }

    std::cout << "\n=== Simulation Results (AT) ===\n";
    std::cout << "Wall time:    " << std::fixed << std::setprecision(3) << elapsed.count() << " s\n";
    std::cout << "Sim time:     " << sc_core::sc_time_stamp() << "\n";
    std::cout << "Instructions: " << perf->getInstructions() << "\n";

    // Print 2-stage pipeline statistics
#if defined(ENABLE_PIPELINED_ISS)
    if (g_top && g_top->cpu && g_top->cpu->isPipelined()) {
        std::cout << "\n=== Pipeline Statistics (2-stage AT) ===\n";
        
        // Try 2-stage RV64
        auto* cpu64p2 = dynamic_cast<riscv_tlm::CPURV64P2*>(g_top->cpu);
        if (cpu64p2) {
            auto stats = cpu64p2->getStats();
            std::cout << "  Pipeline cycles:    " << stats.cycles << "\n";
            std::cout << "  Pipeline stalls:    " << stats.stalls << "\n";
            std::cout << "  Pipeline flushes:   " << stats.flushes << "\n";
            std::cout << "  Control hazards:    " << stats.control_hazards << "\n";
            if (stats.cycles > 0) {
                double ipc = static_cast<double>(perf->getInstructions()) / stats.cycles;
                std::cout << "  IPC:                " << std::fixed << std::setprecision(3) << ipc << "\n";
                double cpi = static_cast<double>(stats.cycles) / perf->getInstructions();
                std::cout << "  CPI:                " << std::fixed << std::setprecision(3) << cpi << "\n";
            }
        }
        
        // Try 2-stage RV32
        auto* cpu32p2 = dynamic_cast<riscv_tlm::CPURV32P2*>(g_top->cpu);
        if (cpu32p2) {
            auto stats = cpu32p2->getStats();
            std::cout << "  Pipeline cycles:    " << stats.cycles << "\n";
            std::cout << "  Pipeline stalls:    " << stats.stalls << "\n";
            std::cout << "  Pipeline flushes:   " << stats.flushes << "\n";
            std::cout << "  Control hazards:    " << stats.control_hazards << "\n";
            if (stats.cycles > 0) {
                double ipc = static_cast<double>(perf->getInstructions()) / stats.cycles;
                std::cout << "  IPC:                " << std::fixed << std::setprecision(3) << ipc << "\n";
                double cpi = static_cast<double>(stats.cycles) / perf->getInstructions();
                std::cout << "  CPI:                " << std::fixed << std::setprecision(3) << cpi << "\n";
            }
        }
    }
#endif

    delete g_top;
    g_top = nullptr;

    return 0;
}