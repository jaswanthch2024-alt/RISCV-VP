# RISC-V TLM Simulator

A complete RISC-V Instruction Set Architecture (ISA) simulator built with SystemC and TLM-2.0 for educational, development, and research purposes.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![SystemC](https://img.shields.io/badge/SystemC-2.3.3+-green.svg)](https://www.accellera.org/downloads/standards/systemc)

---

## 🎯 Overview

This repository contains a **production-ready RISC-V simulator** that supports both 32-bit and 64-bit architectures with multiple instruction set extensions. Built using SystemC and Transaction Level Modeling (TLM-2.0), it provides cycle-accurate simulation suitable for software development, architectural exploration, and education.

### Key Features

- ✅ **Full RISC-V Support**: RV32IMAC and RV64IMAC instruction sets
- ✅ **High Performance**: 3-4.5 million instructions/second
- ✅ **Multiple CPU Models**: Single-cycle and 2-stage pipelined implementations
- ✅ **Rich Peripherals**: Timer, UART, DMA, CLINT, PLIC, Trace output
- ✅ **Debug Support**: GDB remote debugging interface
- ✅ **FreeRTOS Compatible**: Validated with real-time operating systems
- ✅ **Comprehensive Testing**: Validated against official RISC-V test suites
- ✅ **Docker Support**: Pre-built containers available

---

## 📋 Table of Contents

- [Quick Start](#-quick-start)
- [Supported Features](#-supported-features)
- [System Requirements](#-system-requirements)
- [Installation](#-installation)
- [Building](#-building)
- [Usage](#-usage)
- [Documentation](#-documentation)
- [Project Structure](#-project-structure)
- [Testing](#-testing)
- [Performance](#-performance)
- [Contributing](#-contributing)
- [License](#-license)

---

## 🚀 Quick Start

Get up and running in 3 steps:

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/jaswanthch2024-alt/RISC-V-TLM.git
cd RISC-V-TLM

# 2. Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 3. Run a test program
./RISCV_VP -f ../tests/hex/robust_system_test.hex -R 32 -L 3
```

**That's it!** The simulator will execute your RISC-V program and display output in a new xterm window.

---

## ⭐ Supported Features

### RISC-V Architectures
- **RV32IMAC**: 32-bit with Integer, Multiply/Divide, Atomic, Compressed extensions
- **RV64IMAC**: 64-bit with Integer, Multiply/Divide, Atomic, Compressed extensions

### Instruction Set Extensions

| Extension | Description | Status |
|-----------|-------------|--------|
| **I** | Base Integer Instructions | ✅ Complete |
| **M** | Integer Multiplication and Division | ✅ Complete |
| **A** | Atomic Instructions | ✅ Complete |
| **C** | Compressed Instructions (16-bit) | ✅ Complete |
| **F** | Single-Precision Floating-Point | 🚧 In Progress |
| **Zifencei** | Instruction-Fetch Fence | ✅ Complete |
| **Zicsr** | Control and Status Register Instructions | ✅ Complete |

### Hardware Components

- **CPU**: Single-cycle and pipelined (2-stage) implementations
- **Memory**: 16 MB TLM-2.0 based memory with DMI support
- **Registers**: 32 general-purpose registers + PC + CSRs
- **Peripherals**:
  - **Trace**: Debug output to xterm (0x40000000)
  - **Timer**: Programmable timer with interrupts (0x40004000)
  - **UART**: Serial communication (0x10000000)
  - **CLINT**: Core-Local Interruptor (0x02000000)
  - **PLIC**: Platform-Level Interrupt Controller (0x0C000000)
  - **DMA**: Direct Memory Access controller (0x30000000)

### Debug & Development Tools

- **GDB Server**: Remote debugging with `riscv32/64-unknown-elf-gdb`
- **Performance Counters**: Instruction and cycle counting (mcycle, minstret)
- **Logging**: Multi-level logging with spdlog
- **Trace Output**: Real-time program output via Trace peripheral

---

## 💻 System Requirements

### Supported Operating Systems
- Linux (Ubuntu 18.04+, Debian 9+, Fedora 28+)
- Windows 10/11 (Visual Studio 2019+ or WSL2)
- macOS 10.14+ (with Xcode Command Line Tools)

### Required Software
- **CMake** 3.10 or higher
- **C++ Compiler** with C++17 support:
  - GCC 7.0+ or Clang 6.0+ (Linux/macOS)
  - Visual Studio 2019+ (Windows)
- **Git** 2.13+ (for submodules)
- **Make** or **Ninja** build tool

### Recommended Hardware
- Multi-core CPU (Intel i5 or AMD equivalent)
- 4 GB RAM minimum (8 GB recommended)
- 2 GB disk space

---

## 📦 Installation

### Clone the Repository

```bash
# Clone with submodules (REQUIRED)
git clone --recurse-submodules https://github.com/jaswanthch2024-alt/RISC-V-TLM.git
cd RISC-V-TLM

# If you already cloned without submodules:
git submodule update --init --recursive
```

### Install System Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git
```

**Fedora/RHEL:**
```bash
sudo dnf install -y gcc gcc-c++ cmake git
```

**macOS:**
```bash
brew install cmake git
```

---

## 🔨 Building

### Standard Build

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Build Options

Customize your build with CMake options:

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_PIPELINED_ISS=ON \
      -DBUILD_DOC=OFF \
      -DBUILD_TESTING=OFF \
      ..
```

**Available Options:**

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_PIPELINED_ISS` | ON | Enable 2-stage pipelined CPU |
| `BUILD_DOC` | ON | Build Doxygen documentation |
| `BUILD_TESTING` | OFF | Build test suite |
| `ENABLE_STRICT` | OFF | Treat warnings as errors |
| `USE_LOCAL_SYSTEMC` | ON | Use bundled SystemC submodule |
| `BUILD_ROBUST_HEX` | ON | Build test hex programs |

### Build Outputs

After building, you'll have:
- **RISCV_TLM**: Legacy simulator executable
- **RISCV_VP**: Virtual Prototype executable *(recommended)*
- **riscv_tlm_core**: Core library

---

## 🎮 Usage

### Running the Simulator

```bash
# Basic usage
./RISCV_VP -f program.hex -R 32

# With detailed logging
./RISCV_VP -f program.hex -R 64 -L 3

# Enable GDB debugging
./RISCV_VP -f program.hex -R 32 -D
```

### Command-Line Arguments

| Argument | Description | Example |
|----------|-------------|---------|
| `-f <file>` | Hex file to execute | `-f test.hex` |
| `-R <32 or 64>` | Architecture (32-bit or 64-bit) | `-R 32` |
| `-L <level>` | Log level (0=ERROR, 3=INFO) | `-L 3` |
| `-D` | Enable debug mode (GDB server) | `-D` |

### Compiling RISC-V Programs

To run your own programs, you need a RISC-V cross-compiler:

```bash
# For RV32
riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -O2 \
    -nostdlib -Wl,--entry=main program.c -o program.elf
riscv32-unknown-elf-objcopy -O ihex program.elf program.hex

# For RV64
riscv64-unknown-elf-gcc -march=rv64imac -mabi=lp64 -O2 \
    -nostdlib -Wl,--entry=main program.c -o program.elf
riscv64-unknown-elf-objcopy -O ihex program.elf program.hex
```

See [BUILD_GUIDE.md](BUILD_GUIDE.md) for cross-compiler installation instructions.

---

## 📚 Documentation

This repository includes comprehensive documentation:

| Document | Description | Target Audience |
|----------|-------------|-----------------|
| **[README.md](README.md)** | This file - quick start and overview | Everyone |
| **[BUILD_GUIDE.md](BUILD_GUIDE.md)** | Complete build instructions with troubleshooting | Developers, Supervisors |
| **[REPOSITORY_SUMMARY.md](REPOSITORY_SUMMARY.md)** | Technical analysis and component details | Developers, Contributors |
| **[CURRENT_STATUS.md](CURRENT_STATUS.md)** | Current repository state and roadmap | Developers, Users |
| **Doxygen** | API documentation (build with `make doc`) | Developers |

### Quick Reference

- **First-time users**: Start here (README.md)
- **Building the project**: See [BUILD_GUIDE.md](BUILD_GUIDE.md)
- **Understanding the code**: See [REPOSITORY_SUMMARY.md](REPOSITORY_SUMMARY.md)
- **Current features**: See [CURRENT_STATUS.md](CURRENT_STATUS.md)

---

## 🗂️ Project Structure

```
RISC-V-TLM/
├── inc/                    # Header files (24 files)
│   ├── CPU.h              # Main CPU interface
│   ├── BASE_ISA.h         # Base integer instruction set
│   ├── M_extension.h      # Multiply/divide extension
│   ├── A_extension.h      # Atomic operations extension
│   ├── C_extension.h      # Compressed instructions extension
│   ├── Memory.h           # TLM-2 memory model
│   ├── Registers.h        # Register file
│   ├── BusCtrl.h          # Bus interconnect
│   └── ...                # Peripherals, debug, utilities
│
├── src/                    # Implementation files (21 files)
│   ├── CPU.cpp
│   ├── BASE_ISA.cpp
│   ├── M_extension.cpp
│   ├── Simulator.cpp      # Legacy simulator main
│   ├── VPMain.cpp         # Virtual Platform main
│   └── ...
│
├── tests/                  # Test programs
│   ├── full_system/
│   │   └── robust_system_test.c
│   └── vp_overall_test.cpp
│
├── spdlog/                 # Logging library (submodule)
├── systemc/                # SystemC library (submodule)
│
├── BUILD_GUIDE.md          # Build instructions
├── REPOSITORY_SUMMARY.md   # Technical documentation
├── CURRENT_STATUS.md       # Current status
├── CMakeLists.txt          # Build configuration
└── README.md               # This file
```

**Code Metrics:**
- ~9,246 lines of C++ code
- 24 header files
- 21 implementation files

---

## 🧪 Testing

### Test Coverage

- ✅ **riscv-tests**: Official RISC-V ISA tests (almost complete)
- ✅ **riscv-compliance**: Compliance test suite (complete)
- ✅ **FreeRTOS**: Real-time OS validation
- ✅ **Custom tests**: Comprehensive system tests

### Running Tests

```bash
# Build test programs (requires RISC-V cross-compiler)
cmake -DBUILD_ROBUST_HEX=ON ..
make robust_rv32_hex

# Run test
./RISCV_VP -f ../tests/hex/robust_system_test.hex -R 32 -L 3
```

---

## 📈 Performance

**Simulation Speed:**
- Intel i5-5200 @ 2.2GHz: ~3.0 million instructions/second
- Intel i7-8550U @ 1.8GHz: ~4.5 million instructions/second

**Performance Tips:**
- Build with `-DCMAKE_BUILD_TYPE=Release` for optimizations
- Use pipelined ISS: `-DENABLE_PIPELINED_ISS=ON`
- Reduce logging: `-L 0` or `-L 1`
- Use parallel builds: `make -j$(nproc)`

---

## 🐳 Docker Alternative

Don't want to build from source? Use our pre-built Docker image:

```bash
# Pull the image
docker pull mariusmm/riscv-tlm

# Run with your hex file
docker run -v /path/to/hexfiles:/tmp \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -it mariusmm/riscv-tlm \
    /usr/src/riscv64/RISC-V-TLM/RISCV_TLM /tmp/your_program.hex
```

---

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

### Ways to Contribute
- 🐛 **Testing**: Report bugs and test on different platforms
- 💻 **Code**: Submit pull requests for features or fixes
- 📖 **Documentation**: Improve guides and examples
- 🔬 **Research**: Add RTL-level simulation support

### Development Guidelines
- Follow existing code style
- Add tests for new features
- Update documentation
- Ensure backward compatibility

### Getting Started
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test thoroughly
5. Submit a pull request

---

## 🎓 Use Cases

This simulator is perfect for:

1. **RISC-V Software Development**
   - Develop and test RISC-V programs
   - Bare-metal and RTOS development
   - Cross-platform workflow

2. **Computer Architecture Education**
   - Learn RISC-V instruction set
   - Understand CPU design
   - Study memory hierarchies

3. **Embedded System Prototyping**
   - Virtual platform for early development
   - Test peripheral interactions
   - Validate system behavior

4. **Performance Analysis**
   - Instruction profiling
   - Cycle-accurate simulation
   - Memory access patterns

---

## 🔧 Troubleshooting

### Common Issues

**Problem: "Cannot find SystemC"**
```bash
# Solution: Initialize submodules
git submodule update --init --recursive
```

**Problem: "C++17 not supported"**
```bash
# Solution: Upgrade your compiler
# Ubuntu example:
sudo apt-get install gcc-9 g++-9
export CXX=g++-9
```

**Problem: "Cannot open display"**
```bash
# Solution: Set DISPLAY environment variable
export DISPLAY=:0

# Or use Xvfb for headless systems
Xvfb :99 -screen 0 1024x768x16 &
export DISPLAY=:99
```

For more troubleshooting, see [BUILD_GUIDE.md](BUILD_GUIDE.md#troubleshooting).

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0 or later**.

```
Copyright (C) 2018-2025 Màrius Montón

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
```

See the [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0) for full license details.

---

## 👤 Authors

- **Original Author**: Màrius Montón ([@mariusmonton](https://twitter.com/mariusmonton))
- **Fork Maintainer**: @jaswanthch2024-alt

### Citation

If you use this simulator in your research, please cite:

```bibtex
@inproceedings{montonriscvtlm2020,
    title = {A {RISC}-{V} {SystemC}-{TLM} simulator},
    booktitle = {Workshop on {Computer} {Architecture} {Research} with {RISC}-{V} ({CARRV 2020})},
    author = {Montón, Màrius},
    year = {2020}
}
```

---

## 🔗 Links & Resources

- **Original Project**: https://github.com/mariusmm/RISC-V-TLM
- **RISC-V Foundation**: https://riscv.org/
- **RISC-V Specifications**: https://riscv.org/technical/specifications/
- **SystemC**: https://www.accellera.org/downloads/standards/systemc
- **Docker Hub**: https://hub.docker.com/r/mariusmm/riscv-tlm

---

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/jaswanthch2024-alt/RISC-V-TLM/issues)
- **Discussions**: [GitHub Discussions](https://github.com/jaswanthch2024-alt/RISC-V-TLM/discussions)
- **Documentation**: See docs in this repository

---



</div>
