# RISC-V TLM Repository - Complete Summary

## Current Repository Status (December 2025)

This repository contains a **RISC-V ISA Simulator** implemented using SystemC and TLM-2.0. Below is a complete summary of what's included.

---

## 📁 Repository Contents

### Documentation Files
1. **README.md** (12,394 bytes)
   - Original project documentation
   - Installation instructions
   - Usage examples
   - Feature list and TODO items

2. **REPOSITORY_SUMMARY.md** (12,026 bytes)
   - Technical analysis of the codebase
   - Component breakdown
   - Architecture details
   - Memory map and peripheral descriptions

3. **BUILD_GUIDE.md** (10,610 bytes)
   - Step-by-step build instructions for supervisors
   - System requirements
   - Dependency installation
   - Troubleshooting guide

4. **CMakeLists.txt** (7,541 bytes)
   - Build system configuration
   - Compilation options
   - Target definitions

---

## 🏗️ Project Structure

### Source Code Organization

```
RISC-V-TLM/
├── inc/                    # Header files (24 files)
│   ├── CPU.h              # Main CPU interface
│   ├── BASE_ISA.h         # Base integer instruction set
│   ├── M_extension.h      # Multiply/divide extension
│   ├── A_extension.h      # Atomic operations extension
│   ├── C_extension.h      # Compressed instructions extension
│   ├── F_extension.h      # Floating-point extension
│   ├── Registers.h        # Register file (x0-x31, PC, CSRs)
│   ├── Memory.h           # TLM-2 memory model
│   ├── Instruction.h      # Instruction decoder
│   ├── BusCtrl.h          # Bus interconnect
│   ├── Trace.h            # Debug output peripheral
│   ├── Timer.h            # Timer with interrupts
│   ├── UART.h             # UART peripheral
│   ├── CLINT.h            # Core-Local Interruptor
│   ├── PLIC.h             # Platform-Level Interrupt Controller
│   ├── DMA.h              # Direct Memory Access controller
│   ├── Debug.h            # GDB debugging support
│   ├── Performance.h      # Performance counters
│   ├── CPU_P32_2.h        # 2-stage pipeline (RV32)
│   ├── CPU_P64_2.h        # 2-stage pipeline (RV64)
│   ├── VPTop.h            # Virtual Platform top-level
│   └── ...
│
├── src/                    # Implementation files (21 files)
│   ├── CPU.cpp
│   ├── BASE_ISA.cpp
│   ├── M_extension.cpp
│   ├── A_extension.cpp
│   ├── C_extension.cpp
│   ├── F_extension.cpp
│   ├── Registers.cpp
│   ├── Memory.cpp
│   ├── Instruction.cpp
│   ├── BusCtrl.cpp
│   ├── Trace.cpp
│   ├── Timer.cpp
│   ├── Debug.cpp
│   ├── Performance.cpp
│   ├── CPU_P32_2.cpp
│   ├── CPU_P64_2.cpp
│   ├── Simulator.cpp      # Legacy simulator main
│   ├── VPMain.cpp         # Virtual Platform main
│   ├── VPTop.cpp
│   └── ...
│
├── tests/                  # Test programs
│   ├── full_system/
│   │   └── robust_system_test.c   # Comprehensive system test
│   └── vp_overall_test.cpp        # VP integration test
│
├── spdlog/                 # Logging library (git submodule)
├── systemc/                # SystemC library (git submodule)
│
├── BUILD_GUIDE.md          # Build instructions (NEW)
├── REPOSITORY_SUMMARY.md   # Technical summary (NEW)
├── README.md               # Project README
└── CMakeLists.txt          # CMake build configuration
```

---

## 🎯 Key Features

### Supported Architectures
- **RV32IMAC**: 32-bit RISC-V with Integer, Multiply, Atomic, Compressed extensions
- **RV64IMAC**: 64-bit RISC-V with Integer, Multiply, Atomic, Compressed extensions

### CPU Implementations
1. **Single-cycle CPU** (CPU.cpp)
   - Simple, functional implementation
   - Good for debugging and learning

2. **2-stage Pipelined CPU** (CPU_P32_2.cpp, CPU_P64_2.cpp)
   - Improved performance
   - More realistic timing model

### Instruction Set Extensions

| Extension | Description | Status |
|-----------|-------------|--------|
| **I** | Base Integer | ✅ Complete |
| **M** | Multiply/Divide | ✅ Complete |
| **A** | Atomic Operations | ✅ Complete |
| **C** | Compressed (16-bit) | ✅ Complete |
| **F** | Floating-Point | 🚧 In Progress |
| **Zifencei** | Instruction Fence | ✅ Complete |
| **Zicsr** | CSR Instructions | ✅ Complete |

### Memory System
- **Size**: 16 MB (configurable)
- **Model**: TLM-2.0 based
- **Features**:
  - Intel HEX file loading
  - Partial ELF support
  - Direct Memory Interface (DMI) support
  - Fast memory access

### Peripherals (TLM-2.0 Models)

| Peripheral | Base Address | Description |
|------------|--------------|-------------|
| **Trace** | 0x40000000 | Character output to xterm window |
| **Timer** | 0x40004000 | Programmable timer with IRQ support |
| **UART** | 0x10000000 | Serial communication interface |
| **CLINT** | 0x02000000 | Core-Local Interruptor |
| **PLIC** | 0x0C000000 | Platform-Level Interrupt Controller |
| **DMA** | 0x30000000 | Direct Memory Access controller |

### Debug & Development
- **GDB Server**: Remote debugging support (beta)
- **Performance Monitoring**: Instruction and cycle counting
- **Logging**: Multi-level logging with spdlog
- **Trace Output**: Real-time program output via Trace peripheral

---

## 📊 Code Metrics

- **Total Lines of Code**: ~9,246 (headers + implementation)
- **Header Files**: 24 files in `inc/`
- **Source Files**: 21 files in `src/`
- **Language**: C++17 (C++20 on MSVC)
- **Build System**: CMake 3.10+
- **License**: GNU GPL 3.0

---

## 🔧 Build Configuration

### Dependencies
1. **SystemC** (v2.3.3/2.3.4/3.0.0) - Included as submodule
2. **spdlog** - Fast C++ logging library - Included as submodule
3. **Boost** (headers only) - Optional
4. **CMake** 3.10+ - Required
5. **C++17 Compiler** - GCC 7+, Clang 6+, MSVC 2019+

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_PIPELINED_ISS` | ON | Enable 2-stage pipelined CPU |
| `BUILD_DOC` | ON | Build Doxygen documentation |
| `BUILD_TESTING` | OFF | Build test suite |
| `ENABLE_STRICT` | OFF | Treat warnings as errors |
| `USE_LOCAL_SYSTEMC` | ON | Use bundled SystemC submodule |
| `BUILD_ROBUST_HEX` | ON | Build test hex programs |

### Build Targets
- **RISCV_TLM**: Legacy simulator executable
- **RISCV_VP**: Virtual Prototype executable (recommended)
- **riscv_tlm_core**: Core library with all modules
- **doc**: Doxygen documentation

---

## 🚀 Quick Start

### 1. Clone and Build
```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/jaswanthch2024-alt/RISC-V-TLM.git
cd RISC-V-TLM

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 2. Run Simulator
```bash
# Run a RISC-V program
./RISCV_VP -f program.hex -R 32 -L 3

# Arguments:
#   -f <file>     : Hex file to execute
#   -R <32|64>    : Architecture (32 or 64 bit)
#   -L <level>    : Log level (0=ERROR, 3=INFO)
#   -D            : Enable debug mode (GDB server)
```

### 3. Cross-Compile Programs
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

---

## 🧪 Testing

### Test Coverage
- ✅ **riscv-tests**: Official RISC-V ISA tests (almost complete)
- ✅ **riscv-compliance**: Compliance test suite (complete)
- ✅ **FreeRTOS**: Real-time OS support validated
- ✅ **Custom tests**: Comprehensive system tests included

### Test Files
- `tests/full_system/robust_system_test.c`: Tests timer IRQs, DMA, atomics, M-extension
- `tests/vp_overall_test.cpp`: Virtual Platform integration test

---

## 📈 Performance

- **Simulation Speed**: 3-4.5 million instructions/second
  - Intel i5-5200 @ 2.2GHz: ~3.0 MIPS
  - Intel i7-8550U @ 1.8GHz: ~4.5 MIPS
- **Performance varies with**:
  - CPU model used (single-cycle vs. pipelined)
  - Logging level
  - Build optimization level

---

## 🔍 Advanced Features

### Debug Support
- GDB remote debugging via `-D` flag
- Compatible with `riscv32-unknown-elf-gdb` v8.3.0+
- Eclipse integration supported
- Step-through debugging capability

### Operating System Support
- **FreeRTOS**: Full support with port files included
- **Bare-metal**: Standard library support via newlib-nano
- **System calls**: Helper functions for stdio operations

### Docker Support
- Pre-built image: `mariusmm/riscv-tlm`
- No performance penalty
- Includes all dependencies
- Easy cross-platform usage

---

## 📚 Documentation

### Available Documentation
1. **README.md**: User guide and features
2. **REPOSITORY_SUMMARY.md**: Technical analysis and component details
3. **BUILD_GUIDE.md**: Complete build instructions with troubleshooting
4. **Doxygen docs**: API documentation (build with `make doc`)
5. **Code comments**: Inline documentation throughout codebase

### External Resources
- RISC-V Specification: https://riscv.org/technical/specifications/
- SystemC Documentation: https://www.accellera.org/downloads/standards/systemc
- Original Project: https://github.com/mariusmm/RISC-V-TLM

---

## 🎓 Use Cases

1. **RISC-V Software Development**
   - Develop and test RISC-V programs
   - Bare-metal and RTOS development
   - Cross-platform development workflow

2. **Computer Architecture Education**
   - Learn RISC-V instruction set
   - Understand CPU design concepts
   - Study memory hierarchies and peripherals

3. **Embedded System Prototyping**
   - Virtual platform for early development
   - Test peripheral interactions
   - Validate system behavior

4. **Performance Analysis**
   - Instruction profiling
   - Cycle-accurate simulation (with pipelined model)
   - Memory access patterns

---

## 🏆 Quality & Validation

### Testing
- ✅ Travis CI integration
- ✅ Codacy code quality analysis
- ✅ Coverity static analysis
- ✅ Official RISC-V test suite validation

### Code Quality
- Modern C++17 design
- Modular architecture
- Comprehensive error handling
- Extensive inline documentation

---

## 🤝 Contributing

### Ways to Contribute
- Testing and bug reports
- Pull requests (see TODO list in README)
- Documentation improvements
- RTL-level simulation additions

### Development Guidelines
- Follow existing code style
- Add tests for new features
- Update documentation
- Maintain backward compatibility

---

## 📝 Recent Changes (December 2025)

### New Documentation
1. ✅ **BUILD_GUIDE.md** added
   - Complete build instructions
   - System requirements
   - Troubleshooting guide
   - Cross-compiler setup

2. ✅ **REPOSITORY_SUMMARY.md** added
   - Technical analysis
   - Component catalog
   - Architecture overview
   - Memory map details

### Documentation Improvements
- Fixed peripheral implementation details
- Updated memory map accuracy
- Corrected test directory structure
- Enhanced markdown formatting

---

## 🔮 Future Roadmap

### Planned Features
- [ ] Full ELF file loading support
- [ ] Generic IRQ controller
- [ ] Additional UART models
- [ ] Trace v2.0 support
- [ ] Enhanced module hierarchy
- [ ] More peripheral models

### Under Development
- 🚧 F extension (Floating-point)
- 🚧 Improved debugging capabilities
- 🚧 Performance optimizations

---

## 👤 Author & License

- **Original Author**: Màrius Montón (@mariusmonton)
- **Fork Maintainer**: @jaswanthch2024-alt
- **License**: GNU General Public License v3.0 or later
- **Copyright**: 2018-2025

---

## 📞 Support

- **Issues**: https://github.com/jaswanthch2024-alt/RISC-V-TLM/issues
- **Original Project**: https://github.com/mariusmm/RISC-V-TLM
- **Documentation**: See BUILD_GUIDE.md and REPOSITORY_SUMMARY.md

---

## 🎯 Summary

This repository provides a **complete, production-ready RISC-V simulator** with:
- ✅ Full RV32/RV64IMAC support
- ✅ SystemC/TLM-2.0 based design
- ✅ Multiple CPU models (single-cycle and pipelined)
- ✅ Rich peripheral set
- ✅ Debug and profiling tools
- ✅ Comprehensive documentation
- ✅ Validated against official tests
- ✅ Active development and maintenance

**Perfect for**: Education, software development, prototyping, and architectural exploration.

---

*Last Updated: December 18, 2025*  
*Summary Version: 2.0*
