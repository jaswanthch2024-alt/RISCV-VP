#!/usr/bin/env bash
# Complete WSL build and test script for RISC-V-TLM
# This script:
# 1. Checks/installs dependencies
# 2. Builds both pipelined and non-pipelined configurations
# 3. Compiles robust test files to hex format
# 4. Runs comprehensive tests

set -e  # Exit on error

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}=== RISC-V-TLM Complete WSL Build and Test ===${NC}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "\n${YELLOW}Step 1: Checking dependencies...${NC}"

check_command() {
    if ! command -v "$1" &> /dev/null; then
        echo -e "${RED}$1 not found${NC}"
        return 1
    else
        echo -e "${GREEN}? $1 found${NC}"
        return 0
    fi
}

MISSING_DEPS=0
for cmd in gcc g++ cmake make git; do
    check_command "$cmd" || MISSING_DEPS=1
done

if ! check_command "riscv32-unknown-elf-gcc" && ! check_command "riscv64-unknown-elf-gcc"; then
    echo -e "${YELLOW}RISC-V toolchain not found. Installing...${NC}"
    sudo apt update
    sudo apt install -y gcc-riscv64-unknown-elf || {
        echo -e "${RED}Failed to install RISC-V toolchain via apt${NC}"
        echo -e "${YELLOW}Attempting manual installation...${NC}"
        cd /tmp
        TOOLCHAIN_URL="https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v12.2.0-3/xpack-riscv-none-elf-gcc-12.2.0-3-linux-x64.tar.gz"
        wget -O riscv-toolchain.tar.gz "$TOOLCHAIN_URL"
        sudo mkdir -p /opt/riscv
        sudo tar -xzf riscv-toolchain.tar.gz -C /opt/riscv --strip-components=1
        export PATH="/opt/riscv/bin:$PATH"
        if ! grep -q "/opt/riscv/bin" ~/.bashrc; then
            echo 'export PATH="/opt/riscv/bin:$PATH"' >> ~/.bashrc
        fi
        cd "$SCRIPT_DIR"
    }
fi

if [ ! -d "systemc" ] && [ ! -d "/usr/local/systemc-2.3.3" ]; then
    echo -e "${YELLOW}SystemC not found. It will be built from bundled sources.${NC}"
fi

echo -e "\n${YELLOW}Step 2: Building RISC-V-TLM...${NC}"

build_config() {
    local BUILD_DIR="$1"; local PIPE_FLAG="$2"; local CONFIG_NAME="$3"
    echo -e "\n${CYAN}Building $CONFIG_NAME configuration...${NC}"
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        CACHE_SRC=$(awk -F= '/^CMAKE_HOME_DIRECTORY:/{print $2}' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "")
        if [ -n "$CACHE_SRC" ] && [ "$CACHE_SRC" != "$SCRIPT_DIR" ]; then
            echo -e "${YELLOW}Cleaning mismatched CMake cache...${NC}"
            rm -rf "$BUILD_DIR"
        fi
    fi
    mkdir -p "$BUILD_DIR"
    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_PIPELINED_ISS="$PIPE_FLAG" \
        -DUSE_LOCAL_SYSTEMC=ON \
        -DCMAKE_CXX_STANDARD=17 \
        || { echo -e "${RED}CMake configuration failed${NC}"; exit 1; }
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" \
        || { echo -e "${RED}Build failed${NC}"; exit 1; }
    echo -e "${GREEN}? $CONFIG_NAME build complete${NC}"
}

build_config "build_plain" "OFF" "Plain (non-pipelined)"
build_config "build_pipe" "ON" "Pipelined"

echo -e "\n${YELLOW}Step 3: Compiling test programs to hex format...${NC}"

mkdir -p tests/hex
if command -v riscv32-unknown-elf-gcc &> /dev/null; then
    RV32_GCC="riscv32-unknown-elf-gcc"; RV32_OBJCOPY="riscv32-unknown-elf-objcopy"
elif command -v riscv64-unknown-elf-gcc &> /dev/null; then
    RV32_GCC="riscv64-unknown-elf-gcc"; RV32_OBJCOPY="riscv64-unknown-elf-objcopy"
elif [ -x "/opt/riscv/bin/riscv-none-elf-gcc" ]; then
    RV32_GCC="/opt/riscv/bin/riscv-none-elf-gcc"; RV32_OBJCOPY="/opt/riscv/bin/riscv-none-elf-objcopy"
else
    echo -e "${RED}No RISC-V compiler found!${NC}"; exit 1
fi
RV64_GCC="$RV32_GCC"; RV64_OBJCOPY="$RV32_OBJCOPY"
echo -e "${GREEN}Using compiler: $RV32_GCC${NC}"

compile_test() {
    local SOURCE="$1" OUTPUT_BASE="$2" ARCH="$3" ABI="$4" GCC="$5" OBJCOPY="$6"
    if [ ! -f "$SOURCE" ]; then echo -e "${YELLOW}Skipping $SOURCE (file not found)${NC}"; return; fi
    if [ ! -s "$SOURCE" ]; then echo -e "${YELLOW}Skipping $SOURCE (empty)${NC}"; return; fi
    echo -e "${CYAN}Compiling $SOURCE for $ARCH...${NC}"
    "$GCC" -march="$ARCH" -mabi="$ABI" -O2 -nostdlib -Wl,--entry=main -Wl,--gc-sections "$SOURCE" -o "${OUTPUT_BASE}.elf" \
        || { echo -e "${RED}Compilation failed: $SOURCE${NC}"; return 1; }
    "$OBJCOPY" -O ihex "${OUTPUT_BASE}.elf" "${OUTPUT_BASE}.hex" \
        || { echo -e "${RED}objcopy failed: $SOURCE${NC}"; return 1; }
    echo -e "${GREEN}? Created ${OUTPUT_BASE}.hex${NC}"; rm -f "${OUTPUT_BASE}.elf"
}

[ -s tests/full_system/robust_system_test.c ] && compile_test tests/full_system/robust_system_test.c tests/hex/robust_system_test32 rv32imac ilp32 "$RV32_GCC" "$RV32_OBJCOPY"
[ -s tests/full_system/robust_fast.c ] && compile_test tests/full_system/robust_fast.c tests/hex/robust_fast32 rv32imac ilp32 "$RV32_GCC" "$RV32_OBJCOPY"
[ -s tests/full_system/robust_system_test.c ] && compile_test tests/full_system/robust_system_test.c tests/hex/robust_system_test64 rv64imac lp64 "$RV64_GCC" "$RV64_OBJCOPY"
[ -s tests/full_system/robust_fast.c ] && compile_test tests/full_system/robust_fast.c tests/hex/robust_fast64 rv64imac lp64 "$RV64_GCC" "$RV64_OBJCOPY"

echo -e "\n${YELLOW}Step 4: Running tests...${NC}"

run_test() {
    local NAME="$1" EXE="$2" HEX="$3" ARCH="$4"
    if [ ! -f "$HEX" ]; then echo -e "${YELLOW}Skipping $NAME (missing $HEX)${NC}"; return; fi
    echo -e "\n${CYAN}Running: $NAME${NC}"; echo -e "${CYAN}Command: $EXE -f $HEX -R $ARCH${NC}"
    if ! timeout 120 "$EXE" -f "$HEX" -R "$ARCH" 2>&1 | tee "/tmp/test_${NAME}.log"; then
        local EC=$?; [ $EC -eq 124 ] && echo -e "${RED}? Timeout${NC}" || echo -e "${RED}? Failed (exit $EC)${NC}"; return 1; fi
    if grep -q "\[ROBUST\] end\|\[MINIMAL\] end" "/tmp/test_${NAME}.log"; then
        echo -e "${GREEN}? Completed${NC}"; echo -e "${CYAN}Key Metrics:${NC}"; grep -E "accum=|atomic_count=|dma_done=|dma_mismatch=|irq_count=|mcycle=|minstret=" "/tmp/test_${NAME}.log" | sed 's/^/  /'
    else echo -e "${YELLOW}? Possibly incomplete${NC}"; fi
}

TESTS_PASSED=0; TESTS_FAILED=0
for spec in \
  "RV32_Fast_Plain build_plain/RISCV_VP tests/hex/robust_fast32.hex 32" \
  "RV32_Full_Plain build_plain/RISCV_VP tests/hex/robust_system_test32.hex 32" \
  "RV32_Fast_Pipe build_pipe/RISCV_VP tests/hex/robust_fast32.hex 32" \
  "RV32_Full_Pipe build_pipe/RISCV_VP tests/hex/robust_system_test32.hex 32" \
  "RV64_Fast_Plain build_plain/RISCV_VP tests/hex/robust_fast64.hex 64" \
  "RV64_Full_Plain build_plain/RISCV_VP tests/hex/robust_system_test64.hex 64" \
  "RV64_Fast_Pipe build_pipe/RISCV_VP tests/hex/robust_fast64.hex 64" \
  "RV64_Full_Pipe build_pipe/RISCV_VP tests/hex/robust_system_test64.hex 64"; do
  set -- $spec; NAME=$1 EXE=$2 HEX=$3 ARCH=$4
  if run_test "$NAME" "$EXE" "$HEX" "$ARCH"; then ((TESTS_PASSED++)); else ((TESTS_FAILED++)); fi
done

echo -e "\n${CYAN}=== Test Summary ===${NC}"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"; echo -e "${RED}Failed: $TESTS_FAILED${NC}"
[ $TESTS_FAILED -eq 0 ] && echo -e "\n${GREEN}? All tests passed!${NC}" || echo -e "\n${YELLOW}? Some tests failed. Check logs in /tmp/test_*.log${NC}"; [ $TESTS_FAILED -eq 0 ]
