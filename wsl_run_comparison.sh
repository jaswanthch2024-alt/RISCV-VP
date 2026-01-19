#!/usr/bin/env bash
# Comprehensive comparison script for RISC-V-TLM
# Compares TLM and VP simulators with and without pipelining

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Detect if existing build directories contain Windows (PE) binaries; if so, rebuild for Linux
if [ -f build_plain/RISCV_VP ] && file build_plain/RISCV_VP 2>/dev/null | grep -qi 'PE32'; then
  echo -e "${YELLOW}Detected Windows PE binaries in build_plain. Rebuilding for Linux...${NC}"
  rm -rf build_plain build_pipe
fi

if [ -f build_pipe/RISCV_VP ] && file build_pipe/RISCV_VP 2>/dev/null | grep -qi 'PE32'; then
  echo -e "${YELLOW}Detected Windows PE binaries in build_pipe. Rebuilding for Linux...${NC}"
  rm -rf build_pipe
fi

echo -e "${CYAN}======================================================${NC}"
echo -e "${CYAN}  RISC-V-TLM Performance Comparison${NC}"
echo -e "${CYAN}  TLM vs VP | Pipelined vs Non-Pipelined${NC}"
echo -e "${CYAN}======================================================${NC}\n"

# Ensure build script executable
chmod +x wsl_complete_build_and_test.sh || true

# Check if builds exist (Linux ELF); if not, build
needs_build=0
for exe in build_plain/RISCV_VP build_pipe/RISCV_VP; do
  if [ ! -f "$exe" ]; then needs_build=1; continue; fi
  if ! file "$exe" | grep -qi 'ELF'; then needs_build=1; fi
done

if [ $needs_build -eq 1 ]; then
  echo -e "${YELLOW}Linux builds missing or invalid. Running full build...${NC}\n"
  ./wsl_complete_build_and_test.sh || { echo -e "${RED}Build failed${NC}"; exit 1; }
  echo ""
fi

# Find executables
find_exe() {
    local BUILD_DIR="$1"
    local EXE_NAME="$2"
    if [ -f "$BUILD_DIR/$EXE_NAME" ]; then
        echo "$BUILD_DIR/$EXE_NAME"
    elif [ -f "$BUILD_DIR/Release/$EXE_NAME" ]; then
        echo "$BUILD_DIR/Release/$EXE_NAME"
    else
        echo ""
    fi
}

TLM_PLAIN=$(find_exe "build_plain" "RISCV_TLM")
TLM_PIPE=$(find_exe "build_pipe" "RISCV_TLM")
VP_PLAIN=$(find_exe "build_plain" "RISCV_VP")
VP_PIPE=$(find_exe "build_pipe" "RISCV_VP")

MISSING=0
for exe_var in TLM_PLAIN TLM_PIPE VP_PLAIN VP_PIPE; do
    if [ -z "${!exe_var}" ] || [ ! -f "${!exe_var}" ]; then
        echo -e "${RED}? $exe_var executable not found${NC}"
        MISSING=1
    else
        echo -e "${GREEN}? $exe_var: ${!exe_var}${NC}"
    fi
done

if [ $MISSING -eq 1 ]; then
    echo -e "\n${RED}Missing executables. Aborting.${NC}"
    exit 1
fi

echo ""

HEX_FILE="${1:-tests/hex/robust_fast64.hex}"
ARCH="${2:-64}"

if [ ! -f "$HEX_FILE" ]; then
    echo -e "${YELLOW}Hex file not found: $HEX_FILE${NC}"
    for alt in tests/hex/robust_fast64.hex tests/hex/robust_fast32.hex tests/hex/robust_system_test64.hex tests/hex/robust_system_test32.hex; do
        if [ -f "$alt" ]; then
            HEX_FILE="$alt"
            [[ "$alt" == *"32"* ]] && ARCH=32 || ARCH=64
            echo -e "${YELLOW}Using alternative: $HEX_FILE (RV$ARCH)${NC}"
            break
        fi
    done
    if [ ! -f "$HEX_FILE" ]; then
        echo -e "${RED}No suitable hex file found.${NC}"
        exit 1
    fi
fi

echo -e "${CYAN}Test Configuration:${NC}"
echo -e "  Hex File: ${BLUE}$HEX_FILE${NC}"
echo -e "  Architecture: ${BLUE}RV${ARCH}${NC}\n"

declare -A RESULTS

run_test() {
    local NAME="$1"; local EXE="$2"; local HEX="$3"; local ARCH="$4"; local EXTRA_ARGS="${5:-}"
    echo -e "${MAGENTA}????????????????????????????????????????????????${NC}"
    echo -e "${CYAN}Running: ${YELLOW}$NAME${NC}"
    echo -e "${MAGENTA}????????????????????????????????????????????????${NC}"
    local OUTPUT
    if ! OUTPUT=$(timeout 180 "$EXE" -f "$HEX" -R "$ARCH" $EXTRA_ARGS 2>&1); then
        echo -e "${RED}? Execution failed or timed out${NC}\n"; RESULTS["${NAME}_status"]="FAIL"; return 1
    fi
    echo "$OUTPUT" | grep -E "\[ROBUST\]|\[MINIMAL\]|Cycles:|Instr/Cycle:|Instructions:|Total elapsed" || true
    local CYCLES=$(echo "$OUTPUT" | grep -oP 'Cycles:\s+\K\d+' | head -1)
    local INSTRUCTIONS=$(echo "$OUTPUT" | grep -oP 'Instructions:\s+\K\d+' | head -1)
    local IPC=$(echo "$OUTPUT" | grep -oP 'Instr/Cycle:\s+\K[\d.]+' | head -1)
    local ELAPSED=$(echo "$OUTPUT" | grep -oP 'Total elapsed time:\s+\K[\d.]+' | head -1)
    RESULTS["${NAME}_cycles"]="${CYCLES:-0}"; RESULTS["${NAME}_instructions"]="${INSTRUCTIONS:-0}"; RESULTS["${NAME}_ipc"]="${IPC:-0.0}"; RESULTS["${NAME}_elapsed"]="${ELAPSED:-0.0}"; RESULTS["${NAME}_status"]="PASS"
    if echo "$OUTPUT" | grep -q "\[ROBUST\] end\|\[MINIMAL\] end"; then echo -e "${GREEN}? Completed${NC}\n"; else echo -e "${YELLOW}? Possibly incomplete${NC}\n"; fi
}

echo -e "${CYAN}Running tests...\n${NC}"; run_test "TLM_Plain" "$TLM_PLAIN" "$HEX_FILE" "$ARCH"; run_test "TLM_Pipe" "$TLM_PIPE" "$HEX_FILE" "$ARCH"; run_test "VP_Plain" "$VP_PLAIN" "$HEX_FILE" "$ARCH"; run_test "VP_Pipe" "$VP_PIPE" "$HEX_FILE" "$ARCH"

calc_speedup() { local b=$1 i=$2; if (( $(echo "$b == 0" | bc -l) )); then echo 0; else echo "scale=2; (($i-$b)/$b)*100" | bc -l; fi; }

TLM_SPEEDUP=$(calc_speedup "${RESULTS[TLM_Plain_ipc]}" "${RESULTS[TLM_Pipe_ipc]}")
VP_SPEEDUP=$(calc_speedup "${RESULTS[VP_Plain_ipc]}" "${RESULTS[VP_Pipe_ipc]}")

printf "\n${YELLOW}IPC Results:${NC}\n"
for cfg in TLM_Plain TLM_Pipe VP_Plain VP_Pipe; do printf "  %-10s IPC=%s\n" "$cfg" "${RESULTS[${cfg}_ipc]}"; done

printf "\nPipeline Speedup (IPC):\n  TLM: %s%%\n  VP : %s%%\n" "$TLM_SPEEDUP" "$VP_SPEEDUP"

BEST_CFG=""; BEST_IPC=0
for cfg in TLM_Plain TLM_Pipe VP_Plain VP_Pipe; do ipc="${RESULTS[${cfg}_ipc]}"; if (( $(echo "$ipc > $BEST_IPC" | bc -l) )); then BEST_IPC="$ipc"; BEST_CFG="$cfg"; fi; done
printf "\nBest configuration: %s (IPC=%s)\n" "$BEST_CFG" "$BEST_IPC"

REPORT_FILE="comparison_report_rv${ARCH}_$(date +%Y%m%d_%H%M%S).txt"
{
  echo "RISC-V-TLM RV${ARCH} Comparison"; for cfg in TLM_Plain TLM_Pipe VP_Plain VP_Pipe; do echo "$cfg:"; echo "  Cycles: ${RESULTS[${cfg}_cycles]}"; echo "  Instructions: ${RESULTS[${cfg}_instructions]}"; echo "  IPC: ${RESULTS[${cfg}_ipc]}"; echo "  Time(s): ${RESULTS[${cfg}_elapsed]}"; done; echo "Speedups:"; echo "  TLM pipeline: ${TLM_SPEEDUP}%"; echo "  VP pipeline: ${VP_SPEEDUP}%"; echo "Best: $BEST_CFG (IPC=$BEST_IPC)"; } > "$REPORT_FILE"

echo -e "\n${GREEN}Report saved: $REPORT_FILE${NC}\n"
