# RISC-V TLM Simulator - Build Guide

This guide provides complete step-by-step instructions for building and running the RISC-V TLM simulator from scratch.

## Table of Contents
1. [System Requirements](#system-requirements)
2. [Dependencies Installation](#dependencies-installation)
3. [Building the Simulator](#building-the-simulator)
4. [Running the Simulator](#running-the-simulator)
5. [Optional: Cross-Compiler Setup](#optional-cross-compiler-setup)
6. [Troubleshooting](#troubleshooting)

---

## System Requirements

### Supported Operating Systems
- **Linux**: Ubuntu 18.04+, Debian 9+, Fedora 28+, or similar
- **Windows**: Windows 10/11 with Visual Studio 2019+ or WSL2
- **macOS**: macOS 10.14+ (with Xcode Command Line Tools)

### Required Software
- **CMake**: Version 3.10 or higher
- **C++ Compiler**: 
  - GCC 7.0+ or Clang 6.0+ (Linux/macOS)
  - Visual Studio 2019+ with C++17 support (Windows)
- **Git**: Version 2.13+ (for submodule support)
- **Make** or **Ninja** build tool

### Recommended Hardware
- **CPU**: Multi-core processor (Intel i5 or AMD equivalent)
- **RAM**: Minimum 4 GB, 8 GB recommended
- **Disk Space**: ~2 GB for source code, dependencies, and build artifacts

---

## Dependencies Installation

The simulator requires two main dependencies:
1. **SystemC** (version 2.3.3, 2.3.4, or 3.0.0)
2. **spdlog** (logging library)

Both are included as git submodules, so no separate installation is needed.

### Step 1: Clone the Repository

```bash
# Clone with submodules (recommended)
git clone --recurse-submodules https://github.com/jaswanthch2024-alt/RISC-V-TLM.git
cd RISC-V-TLM

# If you already cloned without --recurse-submodules, initialize submodules now:
git submodule update --init --recursive
```

### Step 2: Install System Dependencies

#### On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git
sudo apt-get install -y libboost-dev  # Optional, for Boost headers
```

#### On Fedora/RHEL/CentOS:
```bash
sudo dnf install -y gcc gcc-c++ cmake git
sudo dnf install -y boost-devel  # Optional
```

#### On macOS:
```bash
# Install Homebrew if not already installed (see https://brew.sh)
brew install cmake git
brew install boost  # Optional
```

#### On Windows (using Visual Studio):
1. Install [Visual Studio 2019 or later](https://visualstudio.microsoft.com/)
   - Select "Desktop development with C++" workload
2. Install [CMake](https://cmake.org/download/) (add to PATH)
3. Install [Git for Windows](https://git-scm.com/download/win)

---

## Building the Simulator

### Quick Build (Recommended for Most Users)

The repository is configured to automatically build all dependencies from the included submodules.

```bash
# From the RISC-V-TLM directory
mkdir build
cd build

# Configure the project
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the simulator (this will take 5-15 minutes on first build)
make -j$(nproc)  # Linux/macOS
# or
cmake --build . --config Release  # Cross-platform alternative
```

**Note**: The first build compiles SystemC from source, which takes several minutes. Subsequent builds are much faster.

### Build Output

After successful build, you'll have these executables in the `build/` directory:
- **RISCV_TLM**: Legacy simulator executable
- **RISCV_VP**: Virtual Prototype executable (recommended)

### Build Options

You can customize the build with CMake options:

```bash
# Example with custom options
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_PIPELINED_ISS=ON \
      -DBUILD_DOC=OFF \
      -DBUILD_TESTING=OFF \
      ..
```

**Available Options:**
- `ENABLE_PIPELINED_ISS`: Enable 2-stage pipelined CPU (default: ON)
- `BUILD_DOC`: Build Doxygen documentation (default: ON, requires Doxygen)
- `BUILD_TESTING`: Build test suite (default: OFF)
- `ENABLE_STRICT`: Treat compiler warnings as errors (default: OFF)
- `USE_LOCAL_SYSTEMC`: Use bundled SystemC submodule (default: ON)
- `BUILD_ROBUST_HEX`: Build test hex programs (default: ON, requires RISC-V toolchain)

---

## Running the Simulator

### Basic Usage

The simulator requires a RISC-V program in Intel HEX format (`.hex` file).

```bash
# Using RISCV_VP (recommended)
./RISCV_VP -f <path/to/program.hex> -R 32    # For RV32 programs
./RISCV_VP -f <path/to/program.hex> -R 64    # For RV64 programs

# Using legacy RISCV_TLM
./RISCV_TLM <path/to/program.hex>
```

### Command-Line Arguments

| Argument | Description | Example |
|----------|-------------|---------|
| `-f <file>` | Hex file to execute | `-f test.hex` |
| `-R <32 or 64>` | Architecture (32-bit or 64-bit) | `-R 32` |
| `-L <level>` | Log level (0=ERROR, 3=INFO) | `-L 3` |
| `-D` | Enable debug mode (GDB server) | `-D` |

### Example: Running a Test Program

```bash
# If you have the RISC-V cross-compiler installed (see next section)
cd build
./RISCV_VP -f ../tests/hex/robust_system_test.hex -R 32 -L 3
```

**Expected Output:**
- A new xterm window will open showing program output (Trace peripheral)
- The main terminal shows simulation logs and statistics
- Program exits when the test completes

### Setting Library Paths (if needed)

If you encounter library loading errors, set the library path:

```bash
# Linux
export LD_LIBRARY_PATH=/path/to/RISC-V-TLM/systemc/build/src:$LD_LIBRARY_PATH

# macOS
export DYLD_LIBRARY_PATH=/path/to/RISC-V-TLM/systemc/build/src:$DYLD_LIBRARY_PATH
```

For a permanent solution, add this to your `~/.bashrc` or `~/.zshrc`.

---

## Optional: Cross-Compiler Setup

To compile your own C/C++ programs for the simulator, you need a RISC-V cross-compiler.

### Installing RISC-V GNU Toolchain

#### Option A: Pre-built Binaries (Quick)

**Ubuntu/Debian:**
```bash
sudo apt-get install gcc-riscv64-unknown-elf
```

**Fedora:**
```bash
sudo dnf install riscv64-gnu-toolchain
```

#### Option B: Build from Source (Full Control)

```bash
# Install prerequisites
sudo apt-get install autoconf automake autotools-dev curl python3 \
                     libmpc-dev libmpfr-dev libgmp-dev gawk build-essential \
                     bison flex texinfo gperf libtool patchutils bc zlib1g-dev \
                     libexpat-dev ninja-build

# Clone and build (takes 1-2 hours)
git clone --recursive https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain

# For RV32 toolchain
./configure --prefix=/opt/riscv --with-arch=rv32imac --with-abi=ilp32
make -j$(nproc)

# For RV64 toolchain (or install both)
./configure --prefix=/opt/riscv64 --with-arch=rv64imac --with-abi=lp64
make -j$(nproc)

# Add to PATH
export PATH=$PATH:/opt/riscv/bin
```

### Compiling Programs for the Simulator

Once you have the cross-compiler:

```bash
# Example: Compile a C program for RV32
riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -O2 \
    -nostdlib -Wl,--entry=main -Wl,--gc-sections \
    my_program.c -o my_program.elf

# Convert to Intel HEX format
riscv32-unknown-elf-objcopy -O ihex my_program.elf my_program.hex

# Run in simulator
./RISCV_VP -f my_program.hex -R 32
```

**For RV64:**
```bash
riscv64-unknown-elf-gcc -march=rv64imac -mabi=lp64 -O2 \
    -nostdlib -Wl,--entry=main -Wl,--gc-sections \
    my_program.c -o my_program.elf

riscv64-unknown-elf-objcopy -O ihex my_program.elf my_program.hex
./RISCV_VP -f my_program.hex -R 64
```

---

## Troubleshooting

### Problem: CMake cannot find SystemC

**Solution 1**: Ensure submodules are initialized:
```bash
git submodule update --init --recursive
```

**Solution 2**: The `USE_LOCAL_SYSTEMC` option should be ON (default):
```bash
cmake -DUSE_LOCAL_SYSTEMC=ON ..
```

### Problem: Build fails with C++17 errors

**Solution**: Ensure your compiler supports C++17:
```bash
# Check GCC version (need 7.0+)
gcc --version

# Check Clang version (need 6.0+)
clang --version

# Upgrade if needed (Ubuntu example)
sudo apt-get install gcc-9 g++-9
export CC=gcc-9
export CXX=g++-9
```

### Problem: "Cannot open display" when running simulator

**Cause**: The Trace peripheral tries to open an xterm window for output.

**Solution 1** (Linux with X11):
```bash
export DISPLAY=:0
```

**Solution 2** (SSH/headless):
```bash
# Use Xvfb (virtual framebuffer)
sudo apt-get install xvfb xterm
Xvfb :99 -screen 0 1024x768x16 &
export DISPLAY=:99
```

**Solution 3** (Docker):
```bash
# Allow X11 forwarding
xhost +local:docker
docker run -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix:rw ...
```

### Problem: Slow simulation performance

**Solutions**:
- Build with `-DCMAKE_BUILD_TYPE=Release` (enables optimizations)
- Use the pipelined ISS: `-DENABLE_PIPELINED_ISS=ON`
- Disable verbose logging: use `-L 0` or `-L 1` when running

### Problem: Build takes too long

**Solution**: Use parallel builds:
```bash
# Use all CPU cores
make -j$(nproc)

# Or specify number of cores (e.g., 4)
make -j4
```

### Problem: Windows build issues

**Solution**: Use Visual Studio CMake integration:
```bash
# From Visual Studio Developer Command Prompt
mkdir build && cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

Or use WSL2 for a Linux environment on Windows.

---

## Quick Start Summary

For the impatient, here's the minimal command sequence:

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/jaswanthch2024-alt/RISC-V-TLM.git
cd RISC-V-TLM

# 2. Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 3. Run (if you have a .hex file)
./RISCV_VP -f your_program.hex -R 32 -L 3
```

---

## Additional Resources

- **Main README**: See [README.md](README.md) for detailed project information
- **Repository Analysis**: See [REPOSITORY_SUMMARY.md](REPOSITORY_SUMMARY.md) for technical details
- **API Documentation**: Build with `make doc` (requires Doxygen)
- **RISC-V Specification**: https://riscv.org/technical/specifications/
- **SystemC Documentation**: https://www.accellera.org/downloads/standards/systemc

---

## Docker Alternative (No Build Required)

If you prefer not to build from source, use the pre-built Docker image:

```bash
# Pull the image
docker pull mariusmm/riscv-tlm

# Run with your hex file
docker run -v /path/to/your/hexfiles:/tmp \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -it mariusmm/riscv-tlm \
    /usr/src/riscv64/RISC-V-TLM/RISCV_TLM /tmp/your_program.hex
```

---

## Support

- **Issues**: Report bugs at https://github.com/jaswanthch2024-alt/RISC-V-TLM/issues
- **Original Project**: https://github.com/mariusmm/RISC-V-TLM
- **License**: GNU GPL v3.0 or later

---

**Last Updated**: December 2025  
**Tested With**: Ubuntu 22.04, Fedora 38, macOS 13, Windows 11 + VS2022
