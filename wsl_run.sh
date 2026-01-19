#!/bin/bash
# Script to build and run the RISC-V-TLM simulator in WSL

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)

# Set environment variables if SYSTEMC_HOME is defined
if [ -n "${SYSTEMC_HOME:-}" ]; then
 export LD_LIBRARY_PATH=$SYSTEMC_HOME/lib-linux64:${LD_LIBRARY_PATH:-}
fi

usage() {
 echo "Usage:" >&2
 echo " $0 compare [hex_file] [max_instr] # build both configs and compare" >&2
 echo " $0 [-- normal args to RISCV_VP ...] # build default tree and run RISCV_VP" >&2
}

# Helper: configure & build a build dir
cmake_configure_build() {
 local dir="$1"; shift
 # If an existing CMake cache was created from a different source path (e.g., Windows path vs WSL path), wipe it
 if [ -f "$dir/CMakeCache.txt" ]; then
 cache_src=$(awk -F= '/^CMAKE_HOME_DIRECTORY:/{print $2}' "$dir/CMakeCache.txt" || true)
 if [ -n "${cache_src:-}" ] && [ "$cache_src" != "$ROOT_DIR" ]; then
 echo "Detected mismatched CMake cache (cache src: $cache_src, current src: $ROOT_DIR). Cleaning $dir" >&2
 rm -rf "$dir"
 fi
 fi
 if [ ! -d "$dir" ]; then mkdir -p "$dir"; fi
 cmake -S "$ROOT_DIR" -B "$dir" -G Ninja "$@"
 cmake --build "$dir" --parallel
}

# Helper: pick exe path for single/multi-config generators
get_bin() {
 local dir="$1"; shift
 local exe="$1"; shift
 if [ -x "$dir/$exe" ]; then echo "$dir/$exe"; return; fi
 if [ -x "$dir/Release/$exe" ]; then echo "$dir/Release/$exe"; return; fi
 if [ -x "$dir/RelWithDebInfo/$exe" ]; then echo "$dir/RelWithDebInfo/$exe"; return; fi
 echo "$dir/$exe"
}

# Helper: run a command and extract instr/sec
parse_ips() { awk '/Simulated [0-9]+/ {ips=$2} END {print ips+0}'; }
run_and_get_ips() {
 echo "> $*" >&2
 "$@" 2>&1 | tee /dev/stderr | parse_ips
}

# Make -f path absolute if needed
make_args_absolute_hex() {
 local out=()
 local i=0
 while (( i < $# )); do
   local arg="${!i}"
   if [[ "$arg" == "-f" ]]; then
     local next_index=$((i+1))
     local hex="${!next_index}"
     if [[ -n "$hex" && "$hex" != /* ]]; then
       hex="$ROOT_DIR/$hex"
     fi
     out+=("-f" "$hex")
     i=$((i+2))
     continue
   fi
   out+=("$arg")
   i=$((i+1))
 done
 echo "${out[@]}"
}

if [ "${1:-}" = "compare" ]; then
 shift
 HEX_FILE=${1:-"$ROOT_DIR/tests/hex/hello_fixed.hex"}
 MAX_INSTR=${2:-200000}

 PIPE_DIR="$ROOT_DIR/build_pipe"
 NOPIPE_DIR="$ROOT_DIR/build_nopipe"

 echo "Configuring and building pipelined (ENABLE_PIPELINED_ISS=ON)" >&2
 cmake_configure_build "$PIPE_DIR" -DENABLE_PIPELINED_ISS=ON -DCMAKE_BUILD_TYPE=Release
 RISCV_TLM_PIPE=$(get_bin "$PIPE_DIR" RISCV_TLM)
 RISCV_VP_PIPE=$(get_bin "$PIPE_DIR" RISCV_VP)

 echo "Configuring and building non-pipelined (ENABLE_PIPELINED_ISS=OFF)" >&2
 cmake_configure_build "$NOPIPE_DIR" -DENABLE_PIPELINED_ISS=OFF -DCMAKE_BUILD_TYPE=Release
 RISCV_TLM_NOPIPE=$(get_bin "$NOPIPE_DIR" RISCV_TLM)

 echo "Running RISCV_TLM (pipelined)" >&2
 IPS_TLM_PIPE=$(run_and_get_ips "$RISCV_TLM_PIPE" -f "$HEX_FILE" --max-instr "$MAX_INSTR")

 echo "Running RISCV_TLM (non-pipelined)" >&2
 IPS_TLM_NOPIPE=$(run_and_get_ips "$RISCV_TLM_NOPIPE" -f "$HEX_FILE" --max-instr "$MAX_INSTR")

 echo "Running RISCV_VP (rv32-pipe=on)" >&2
 IPS_VP_PIPE_ON=$(run_and_get_ips "$RISCV_VP_PIPE" -f "$HEX_FILE" --max-instr "$MAX_INSTR" --rv32-pipe on)

 echo "Running RISCV_VP (rv32-pipe=off)" >&2
 IPS_VP_PIPE_OFF=$(run_and_get_ips "$RISCV_VP_PIPE" -f "$HEX_FILE" --max-instr "$MAX_INSTR" --rv32-pipe off)

 echo
 printf "=== Pipeline vs Non-Pipeline (instr/sec) ===\n"
 printf "TLM pipelined : %'d\n" "$IPS_TLM_PIPE"
 printf "TLM non-pipelined : %'d\n" "$IPS_TLM_NOPIPE"
 printf "VP pipe=on : %'d\n" "$IPS_VP_PIPE_ON"
 printf "VP pipe=off : %'d\n" "$IPS_VP_PIPE_OFF"

 calc_delta() {
 local a="$1" b="$2"
 if [ "${b:-0}" -eq 0 ]; then echo "n/a"; return; fi
 awk -v a="$a" -v b="$b" 'BEGIN { printf "%.2f%%", (a-b)*100.0/b }'
 }

 echo
 printf "Speedup (pipeline vs non):\n"
 printf "TLM: %s\n" "$(calc_delta "$IPS_TLM_PIPE" "$IPS_TLM_NOPIPE")"
 printf "VP : %s\n" "$(calc_delta "$IPS_VP_PIPE_ON" "$IPS_VP_PIPE_OFF")"
 exit 0
fi

# Default path: build and run RISCV_VP with provided args
cd "$ROOT_DIR"
# Clean mismatched cache if needed
if [ -f build/CMakeCache.txt ]; then
 cache_src=$(awk -F= '/^CMAKE_HOME_DIRECTORY:/{print $2}' build/CMakeCache.txt || true)
 if [ -n "${cache_src:-}" ] && [ "$cache_src" != "$ROOT_DIR" ]; then
   echo "Detected mismatched CMake cache in ./build. Removing it to reconfigure under WSL." >&2
   rm -rf build
 fi
fi
if [ ! -d build ]; then
 cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build --parallel

exe=$(get_bin "$ROOT_DIR/build" RISCV_VP)
# Rewrite -f path to absolute if needed
ABS_ARGS=( $(make_args_absolute_hex "$@") )
"$exe" "${ABS_ARGS[@]}"