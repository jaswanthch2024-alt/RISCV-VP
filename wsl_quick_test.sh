#!/bin/bash
# Quick test runner - runs just the fast tests for rapid verification

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${CYAN}=== Quick Test Runner ===${NC}"
echo "Running fast tests only for rapid verification..."
echo

# Check if builds exist
if [ ! -f "build_plain/RISCV_VP" ] && [ ! -f "build_plain/Release/RISCV_VP" ]; then
    echo -e "${YELLOW}Build not found. Running full build first...${NC}"
    ./wsl_complete_build_and_test.sh
    exit $?
fi

# Find the VP executable
if [ -f "build_plain/RISCV_VP" ]; then
    VP_PLAIN="build_plain/RISCV_VP"
elif [ -f "build_plain/Release/RISCV_VP" ]; then
    VP_PLAIN="build_plain/Release/RISCV_VP"
else
    echo "Error: Cannot find RISCV_VP executable"
    exit 1
fi

if [ -f "build_pipe/RISCV_VP" ]; then
    VP_PIPE="build_pipe/RISCV_VP"
elif [ -f "build_pipe/Release/RISCV_VP" ]; then
    VP_PIPE="build_pipe/Release/RISCV_VP"
else
    VP_PIPE="$VP_PLAIN"
fi

run_quick_test() {
    local NAME="$1"
    local EXE="$2"
    local HEX="$3"
    local ARCH="$4"
    
    if [ ! -f "$HEX" ]; then
        echo -e "${YELLOW}Skipping $NAME (hex not found)${NC}"
        return 1
    fi
    
    echo -e "\n${CYAN}Running: $NAME${NC}"
    
    timeout 30 "$EXE" -f "$HEX" -R "$ARCH" 2>&1 | grep -E "\[ROBUST\]|\[MINIMAL\]|Cycles:|Instr/Cycle:" || true
    
    local EXIT_CODE=${PIPESTATUS[0]}
    if [ $EXIT_CODE -eq 0 ]; then
        echo -e "${GREEN}? Pass${NC}"
        return 0
    else
        echo -e "${YELLOW}? Test may have issues (exit code: $EXIT_CODE)${NC}"
        return 1
    fi
}

PASSED=0
FAILED=0

# Run minimal tests first
echo -e "\n${CYAN}=== Minimal Tests ===${NC}"
for hex in tests/hex/robust_minimal*.hex; do
    if [ -f "$hex" ]; then
        if [[ "$hex" == *"32"* ]]; then
            run_quick_test "Minimal-RV32-Plain" "$VP_PLAIN" "$hex" "32" && ((PASSED++)) || ((FAILED++))
        else
            run_quick_test "Minimal-RV64-Plain" "$VP_PLAIN" "$hex" "64" && ((PASSED++)) || ((FAILED++))
        fi
    fi
done

# Run fast tests
echo -e "\n${CYAN}=== Fast Tests ===${NC}"
for hex in tests/hex/robust_fast*.hex; do
    if [ -f "$hex" ]; then
        if [[ "$hex" == *"32"* ]]; then
            run_quick_test "Fast-RV32-Plain" "$VP_PLAIN" "$hex" "32" && ((PASSED++)) || ((FAILED++))
            run_quick_test "Fast-RV32-Pipe" "$VP_PIPE" "$hex" "32" && ((PASSED++)) || ((FAILED++))
        else
            run_quick_test "Fast-RV64-Plain" "$VP_PLAIN" "$hex" "64" && ((PASSED++)) || ((FAILED++))
            run_quick_test "Fast-RV64-Pipe" "$VP_PIPE" "$hex" "64" && ((PASSED++)) || ((FAILED++))
        fi
    fi
done

echo -e "\n${CYAN}=== Quick Test Summary ===${NC}"
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${YELLOW}Failed: $FAILED${NC}"
else
    echo -e "Failed: $FAILED"
fi

if [ $FAILED -eq 0 ] && [ $PASSED -gt 0 ]; then
    echo -e "\n${GREEN}? All quick tests passed!${NC}"
    exit 0
else
    echo -e "\n${YELLOW}? Some tests had issues. Run full test suite for details.${NC}"
    exit 1
fi
