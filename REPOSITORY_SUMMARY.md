# RISC-V TLM Repository - Comprehensive Analysis

## Overview
This repository contains **Another RISC-V ISA simulator** - a full-featured RISC-V instruction set architecture simulator coded in SystemC + TLM-2 (Transaction Level Modeling).

### Key Features
- **Supported Architectures**: RV32IMAC and RV64IMAC instruction sets
- **Technology**: SystemC + TLM-2.0 transaction-level modeling
- **Performance**: ~3-4.5 million instructions/second (varies by CPU)
- **License**: GNU GPL 3.0
- **Status**: Production-ready with ongoing refactoring allowed

## Repository Structure

### Main Directories
```
RISC-V-TLM/
├── inc/              # Header files (24 files)
├── src/              # Implementation files (21 files)
├── tests/            # Test programs and validation
├── spdlog/           # Logging library (git submodule)
├── systemc/          # SystemC library (git submodule)
├── CMakeLists.txt    # Build configuration
└── README.md         # Main documentation
```

### Code Statistics
- **Total Lines**: ~9,246 lines of C++ code (headers + sources)
- **Header Files**: 24 files in `inc/` directory
- **Source Files**: 21 files in `src/` directory

## Core Components

### 1. CPU Module (CPU.h/cpp)
- **Purpose**: Top-level CPU entity
- **Types**: Supports RV32 and RV64 architectures
- **Features**: 
  - Base implementation (single-cycle)
  - Optional pipelined implementation (2-stage)
  - Debug capability support

### 2. Instruction Set Extensions
The simulator implements multiple RISC-V extensions:

- **BASE_ISA** (BASE_ISA.h/cpp)
  - Base integer instructions (I)
  - Zifencei extension (fence instructions)
  - Zicsr extension (CSR operations)

- **C_extension** (C_extension.h/cpp)
  - Compressed 16-bit instructions
  - Reduces code size significantly

- **M_extension** (M_extension.h/cpp)
  - Multiplication and Division operations
  - Integer multiply/divide/remainder

- **A_extension** (A_extension.h/cpp)
  - Atomic operations
  - Load-reserved/store-conditional
  - Atomic memory operations (AMO)

- **F_extension** (F_extension.h/cpp)
  - Floating-point support (in development)

### 3. Memory Subsystem

- **Memory** (Memory.h/cpp)
  - TLM-2 based memory model
  - 16 MB default size (configurable)
  - Supports loading programs from .hex files
  - Partial .elf file support

- **MemoryInterface** (MemoryInterface.h/cpp)
  - Abstraction layer for memory operations
  - TLM-2 transaction handling

- **BusCtrl** (BusCtrl.h/cpp)
  - Simple bus manager/interconnect
  - Routes transactions between CPU and peripherals

### 4. Register File

- **Registers** (Registers.h/cpp)
  - General-purpose registers (x0-x31)
  - Program Counter (PC)
  - Control and Status Registers (CSRs)
  - Supports both RV32 and RV64 register widths

### 5. Instruction Decoder

- **Instruction** (Instruction.h/cpp)
  - Decodes instruction types
  - Extracts instruction fields
  - Identifies opcodes and formats

### 6. Peripherals

The simulator includes several TLM-2 peripherals:

- **Trace** (Trace.h/cpp)
  - Memory-mapped output peripheral
  - Opens xterm window for output
  - Address: 0x40000000

- **Timer** (Timer.h/cpp)
  - Programmable real-time counter
  - Interrupt generation support
  - Memory map:
    - 0x40004000: Timer LSB
    - 0x40004004: Timer MSB
    - 0x40004008: Comparator MSB
    - 0x4000400C: Comparator LSB

- **UART** (UART.h)
  - Standard UART peripheral model
  - Header-only implementation

- **CLINT** (CLINT.h)
  - Core-Local Interruptor
  - Timer interrupts
  - Header-only implementation

- **PLIC** (PLIC.h)
  - Platform-Level Interrupt Controller
  - Header-only implementation

- **DMA** (DMA.h)
  - Direct Memory Access controller
  - Base address: 0x30000000
  - Header-only implementation

### 7. Debug Support

- **Debug** (Debug.h/cpp)
  - GDB server for remote debugging
  - Supports Eclipse and command-line gdb
  - Beta status but functional
  - Compatible with riscv32-unknown-elf-gdb v8.3.0+

### 8. Helper/Utility Classes

- **Performance** (Performance.h/cpp)
  - Singleton class for performance metrics
  - Tracks instructions executed
  - Cycle counting

- **Log** (Log.h/cpp)
  - Centralized logging (uses spdlog)
  - Multiple log levels supported

- **SyscallIf** (SyscallIf.h/cpp)
  - System call interface
  - Handles OS-level calls from programs

### 9. Virtual Prototype

- **VPTop** (VPTop.h/cpp)
  - Virtual Platform top-level
  - Complete system integration

- **VPMain** (VPMain.cpp)
  - Alternative main entry point
  - RISCV_VP executable

## Pipelined Implementation

The repository includes experimental pipelined CPU cores:

- **CPU_P32_2** (CPU_P32_2.h/cpp) - 2-stage pipeline for RV32
- **CPU_P64_2** (CPU_P64_2.h/cpp) - 2-stage pipeline for RV64

Note: 7-stage pipeline variants (CPU_P32/CPU_P64) are excluded from build.

## Build System

### CMake Configuration
- **Build Tool**: CMake 3.10+
- **C++ Standard**: C++17 (C++20 on MSVC)
- **Generator**: Prefers Ninja if available

### Build Options
- `ENABLE_STRICT`: Treat warnings as errors (OFF by default)
- `BUILD_DOC`: Build Doxygen documentation (ON by default)
- `BUILD_TESTING`: Build tests (OFF by default)
- `ENABLE_PIPELINED_ISS`: Enable pipelined cores (ON by default)
- `USE_LOCAL_SYSTEMC`: Use bundled SystemC (ON by default)
- `BUILD_ROBUST_HEX`: Build test hex images (ON by default)

### Dependencies
1. **SystemC** (v2.3.3/2.3.4/3.0.0)
   - Provided as git submodule in `systemc/`
   - Must be compiled with C++17
   
2. **spdlog** (logging library)
   - Provided as git submodule in `spdlog/`
   - Fast C++ logging library

3. **Boost** (headers only)
   - Optional, uses system installation

### Build Targets
- `RISCV_TLM`: Legacy simulator executable
- `RISCV_VP`: Virtual Prototype executable
- `riscv_tlm_core`: Core library with all modules
- `doc`: Generate Doxygen documentation
- `robust_rv32_hex`: Build RV32 test programs
- `robust_rv64_hex`: Build RV64 test programs

## Testing

### Test Directory Structure
```
tests/
├── full_system/
│   └── robust_system_test.c  # Comprehensive system test
└── vp_overall_test.cpp      # VP integration test
```

Note: Compiled hex binaries are created in `tests/hex/` during build if `BUILD_ROBUST_HEX` option is enabled.

### Test Coverage
- **robust_system_test.c**: Comprehensive test covering:
  - Timer interrupts
  - DMA operations
  - Atomic instructions (A extension)
  - M extension (multiply/divide)
  - Memory bandwidth stress
  - Trace peripheral output
  - Performance counters (mcycle, minstret)

### External Test Suites
According to README, the project has been tested with:
- riscv-tests (official RISC-V tests) - almost complete
- riscv-compliance suite - complete

## Usage

### Basic Execution
```bash
./RISCV_TLM <hexfile>
# or
./RISCV_VP -f <hexfile> -R 32  # for RV32
./RISCV_VP -f <hexfile> -R 64  # for RV64
```

### Command-Line Arguments
- `-L <level>`: Log level (0=ERROR, 3=INFO)
- `-f <filename>`: Hex binary to load
- `-D`: Enter debug mode (GDB server)
- `-R <32|64>`: Choose architecture (32-bit or 64-bit)

## Development Workflow

### Compilation Process
```bash
# Setup environment
export SYSTEMC_HOME=<path-to-systemc>
export LD_LIBRARY_PATH=$SYSTEMC_HOME/lib-linux64
export SPDLOG_HOME=<path-to-spdlog>

# Build with CMake
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Cross-Compilation (RISC-V Programs)
Uses official RISC-V GNU toolchain:
```bash
riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 program.c -o program.elf
riscv32-unknown-elf-objcopy -O ihex program.elf program.hex
```

## Notable Features

### 1. FreeRTOS Support
- Fully portable to this simulator
- Port files included in test/FreeRTOS/
- Demonstrates real-time OS capability

### 2. Docker Support
- Pre-built container: `mariusmm/riscv-tlm`
- Includes simulator and dependencies
- No performance penalty

### 3. Standard Library Support
- Links against newlib-nano
- Helper functions for stdio (printf, etc.)
- Redirects stdout to Trace peripheral

### 4. Performance Monitoring
- Instruction counting
- Cycle counting
- Performance metrics collection
- Singleton Performance class

## Memory Map

| Address      | Module | Description             |
|--------------|--------|-------------------------|
| 0x40000000   | Trace  | Output to xterm         |
| 0x40004000   | Timer  | Timer LSB               |
| 0x40004004   | Timer  | Timer MSB               |
| 0x40004008   | Timer  | Timer Comparator LSB    |
| 0x4000400C   | Timer  | Timer Comparator MSB    |
| 0x30000000   | DMA    | DMA Controller base     |
| 0x10000000   | UART   | UART0 base              |
| 0x02000000   | CLINT  | Core-Local Interruptor  |
| 0x0C000000   | PLIC   | Interrupt Controller    |

## Current Status & TODO

### Completed Features ✓
- [x] All RISC-V instructions implemented
- [x] CSR support
- [x] RV64 architecture support
- [x] Debug capabilities (GDB)
- [x] Timer interrupts
- [x] Compressed instructions (C extension)
- [x] riscv-tests validation
- [x] riscv-compliance validation
- [x] SystemC 3.0.0 support

### Pending Features
- [ ] Full .elf file loading support
- [ ] Generic IRQ controller
- [ ] Standard UART models
- [ ] Trace v2.0 support
- [ ] Improved module hierarchy
- [ ] Additional peripherals

## Documentation

### Available Documentation
- **README.md**: Comprehensive user guide
- **Doxygen**: Full API documentation (build with `make doc`)
- **Code Comments**: Inline documentation throughout

### References & Publications
- Workshop paper: "A RISC-V SystemC-TLM simulator" (CARRV 2020)
- Author: Màrius Montón
- DOI: 10.5281/zenodo.7181526

## Project Metadata

### Quality Badges
- Travis CI integration
- Codacy code quality analysis
- Coverity static analysis
- Docker automated builds

### Author & License
- **Author**: Màrius Montón (@mariusmonton)
- **License**: GNU General Public License v3.0 or later
- **Copyright**: 2018-2022

### Contributing
Contributions welcome in:
- Testing
- Pull requests (see TODO list)
- Documentation improvements
- RTL-level simulation

## Architecture Diagram

According to documentation, the module hierarchy follows:
```
Simulator/VPTop
    ├── CPU (RV32/RV64)
    │   ├── BASE_ISA
    │   ├── C_extension
    │   ├── M_extension
    │   ├── A_extension
    │   ├── Registers
    │   └── Instruction decoder
    ├── Memory
    ├── BusCtrl
    │   ├── Trace
    │   ├── Timer
    │   ├── UART
    │   ├── CLINT
    │   ├── PLIC
    │   └── DMA
    ├── Debug (GDB server)
    └── Performance monitor
```

## Git Submodules

The repository uses two submodules:
1. **spdlog**: Fast C++ logging library (https://github.com/gabime/spdlog)
2. **systemc**: SystemC reference implementation (https://github.com/accellera-official/systemc.git)

Initialize with:
```bash
git submodule update --init --recursive
```

## Summary

This is a **complete, working RISC-V simulator** with the following characteristics:

**Strengths:**
- Comprehensive RISC-V ISA support (RV32/RV64 with I, M, A, C extensions)
- TLM-2.0 based design for modularity
- Good performance (3-4.5 MIPS)
- Well-tested against official test suites
- Debug support via GDB
- Docker containerization
- Active development with clean architecture

**Architecture:**
- Transaction-level modeling (TLM-2.0)
- SystemC-based discrete event simulation
- Modular peripheral design
- Support for both functional and pipelined CPU models

**Use Cases:**
- RISC-V software development and testing
- Embedded system prototyping
- Educational tool for computer architecture
- FreeRTOS and bare-metal development
- Architectural exploration and performance analysis

**Code Quality:**
- ~9,200+ lines of well-structured C++
- Comprehensive header/implementation separation
- Extensive use of modern C++ (C++17)
- Automated testing and CI/CD
- Active refactoring allowed per project notes
