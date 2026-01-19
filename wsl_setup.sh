#!/bin/bash
# Setup script - makes all scripts executable and verifies basic environment

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}=== RISC-V-TLM WSL Setup Verification ===${NC}\n"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${YELLOW}1. Making scripts executable...${NC}"
chmod +x wsl_complete_build_and_test.sh 2>/dev/null || true
chmod +x wsl_quick_test.sh 2>/dev/null || true
chmod +x wsl_run.sh 2>/dev/null || true
chmod +x setup_wsl.sh 2>/dev/null || true
chmod +x wsl_setup.sh 2>/dev/null || true
echo -e "${GREEN}? Scripts are now executable${NC}\n"

echo -e "${YELLOW}2. Checking WSL environment...${NC}"
if [ -f /proc/version ] && grep -q Microsoft /proc/version; then
    echo -e "${GREEN}? Running in WSL${NC}"
elif [ -f /proc/version ] && grep -q Linux /proc/version; then
    echo -e "${GREEN}? Running in Linux${NC}"
else
    echo -e "${YELLOW}? Environment not recognized (might be OK)${NC}"
fi

echo -e "${YELLOW}3. Checking basic commands...${NC}"
for cmd in gcc g++ cmake make git; do
    if command -v $cmd &> /dev/null; then
        VERSION=$($cmd --version 2>&1 | head -n1)
        echo -e "${GREEN}? $cmd${NC} - $VERSION"
    else
        echo -e "${RED}? $cmd not found${NC}"
    fi
done

echo -e "\n${YELLOW}4. Checking RISC-V toolchain...${NC}"
RISCV_FOUND=false
for gcc_variant in riscv32-unknown-elf-gcc riscv64-unknown-elf-gcc riscv-none-elf-gcc; do
    if command -v $gcc_variant &> /dev/null; then
        VERSION=$($gcc_variant --version 2>&1 | head -n1)
        echo -e "${GREEN}? $gcc_variant${NC} - $VERSION"
        RISCV_FOUND=true
        break
    fi
done

if [ "$RISCV_FOUND" = false ]; then
    echo -e "${YELLOW}? RISC-V toolchain not found${NC}"
    echo -e "${YELLOW}  The build script will attempt to install it automatically${NC}"
fi

echo -e "\n${YELLOW}5. Checking project structure...${NC}"
MISSING_DIRS=false
for dir in src inc tests systemc spdlog; do
    if [ -d "$dir" ]; then
        echo -e "${GREEN}? $dir/${NC} directory exists"
    else
        echo -e "${RED}? $dir/${NC} directory missing"
        MISSING_DIRS=true
    fi
done

if [ "$MISSING_DIRS" = true ]; then
    echo -e "${RED}\nError: Required directories missing!${NC}"
    echo -e "${YELLOW}Make sure you're in the correct directory.${NC}"
    exit 1
fi

echo -e "\n${YELLOW}6. Checking existing builds...${NC}"
for build_dir in build build_plain build_pipe; do
    if [ -d "$build_dir" ]; then
        if [ -f "$build_dir/RISCV_VP" ] || [ -f "$build_dir/Release/RISCV_VP" ]; then
            echo -e "${GREEN}? $build_dir/${NC} exists with executables"
        else
            echo -e "${YELLOW}? $build_dir/${NC} exists but no executables found"
        fi
    else
        echo -e "  $build_dir/ not built yet"
    fi
done

echo -e "\n${YELLOW}7. Checking test files...${NC}"
TEST_COUNT=0
for test_c in tests/full_system/*.c; do
    if [ -f "$test_c" ] && [ -s "$test_c" ]; then
        echo -e "${GREEN}?${NC} $(basename $test_c)"
        ((TEST_COUNT++))
    fi
done
echo -e "  Found $TEST_COUNT test source files"

HEX_COUNT=0
if [ -d "tests/hex" ]; then
    for hex_file in tests/hex/*.hex; do
        if [ -f "$hex_file" ]; then
            ((HEX_COUNT++))
        fi
    done
    echo -e "  Found $HEX_COUNT compiled hex files"
fi

echo -e "\n${CYAN}=== Setup Summary ===${NC}"
echo -e "Location: ${CYAN}$SCRIPT_DIR${NC}"
echo -e "Scripts: ${GREEN}Executable${NC}"
echo -e "Test programs: ${GREEN}$TEST_COUNT source files, $HEX_COUNT hex files${NC}"

echo -e "\n${CYAN}=== Next Steps ===${NC}"
echo -e "To build and test everything:"
echo -e "  ${GREEN}./wsl_complete_build_and_test.sh${NC}"
echo -e ""
echo -e "For quick verification only:"
echo -e "  ${GREEN}./wsl_quick_test.sh${NC}"
echo -e ""
echo -e "From Windows:"
echo -e "  ${GREEN}wsl_build.bat${NC}"
echo -e ""
echo -e "For documentation:"
echo -e "  ${GREEN}cat WSL_BUILD_GUIDE.md${NC}"
echo -e "  ${GREEN}cat WSL_SETUP_COMPLETE.md${NC}"

echo -e "\n${GREEN}? Setup verification complete!${NC}"
exit 0
