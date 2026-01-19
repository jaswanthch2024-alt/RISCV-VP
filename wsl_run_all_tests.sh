#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run build + all VP tests under WSL, plus direct VP runs for each hex file.
# Usage: ./wsl_run_all_tests.sh [--reconfigure] [--clean] [--debug] [--keep-going]
# Env vars:
#   MAX_INSTR   Instruction cap per VP invocation (default 200000)
#   BUILD_DIR   Build directory (default build)
#   GENERATOR   CMake generator (default Ninja if available else Unix Makefiles)

set -euo pipefail

MAX_INSTR=${MAX_INSTR:-200000}
BUILD_DIR=${BUILD_DIR:-build}
GENERATOR=${GENERATOR:-}
RECONFIG=0
CLEAN=0
DEBUG=0
KEEP_GOING=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reconfigure) RECONFIG=1; shift;;
    --clean) CLEAN=1; shift;;
    --debug) DEBUG=1; shift;;
    --keep-going) KEEP_GOING=1; shift;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# //'; exit 0;;
    *) echo "Unknown arg: $1" >&2; exit 1;;
  esac
done

# Basic WSL/Linux sanity check
if [[ "$(uname -s)" != Linux* ]]; then
  echo "Error: This script must run inside WSL/Linux." >&2
  exit 1
fi

# Prefer Ninja
if [[ -z "$GENERATOR" ]]; then
  if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
  else
    GENERATOR="Unix Makefiles"
  fi
fi

CMAKE_TYPE=Release
[[ $DEBUG -eq 1 ]] && CMAKE_TYPE=Debug

if [[ $CLEAN -eq 1 ]]; then
  echo "[INFO] Removing build directory" && rm -rf "$BUILD_DIR"
fi

if [[ ! -d "$BUILD_DIR" || $RECONFIG -eq 1 ]]; then
  echo "[INFO] Configuring project (generator: $GENERATOR, type: $CMAKE_TYPE)"
  cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE="$CMAKE_TYPE"
fi

echo "[INFO] Building"
if [[ "$GENERATOR" == Ninja ]]; then
  cmake --build "$BUILD_DIR" -j"$(nproc)"
else
  cmake --build "$BUILD_DIR" -- -j"$(nproc)"
fi

LOG_DIR=logs/vp_runs
mkdir -p "$LOG_DIR"

echo "[INFO] Running ctest suite"
set +e
ctest --test-dir "$BUILD_DIR" --output-on-failure
CTEST_RC=$?
set -e

# Collect hex files (exclude known-bad hello.hex as in CMake)
mapfile -t HEX_FILES < <(ls tests/hex/*.hex 2>/dev/null | grep -v '/hello.hex$' || true)
if [[ ${#HEX_FILES[@]} -eq 0 ]]; then
  echo "[WARN] No .hex files found in tests/hex" >&2
fi

VP_BIN="$BUILD_DIR/RISCV_VP"
if [[ ! -x "$VP_BIN" ]]; then
  echo "[ERROR] VP binary not found: $VP_BIN" >&2
  exit 1
fi

PASSED=0
FAILED=0
DETAILS=()

echo "[INFO] Direct VP runs (instruction cap: $MAX_INSTR)"
for hex in "${HEX_FILES[@]}"; do
  name=$(basename "$hex" .hex)
  log="$LOG_DIR/${name}.log"
  echo "  -> $name" | tee "$log"
  if "$VP_BIN" -f "$hex" --max-instr "$MAX_INSTR" > >(tee -a "$log") 2>&1; then
    PASSED=$((PASSED+1))
    DETAILS+=("$name:PASS")
  else
    FAILED=$((FAILED+1))
    DETAILS+=("$name:FAIL")
    [[ $KEEP_GOING -eq 0 ]] && echo "[ERROR] Stopping on first failure (use --keep-going to continue)" && break
  fi
done

echo "================ Summary ================"
echo "CTest return code : $CTEST_RC"
echo "VP hex PASS       : $PASSED"
echo "VP hex FAIL       : $FAILED"
for d in "${DETAILS[@]}"; do echo "  $d"; done

EXIT_CODE=$CTEST_RC
[[ $FAILED -gt 0 ]] && EXIT_CODE=1

if [[ $EXIT_CODE -eq 0 ]]; then
  echo "[INFO] All tests passed"
else
  echo "[INFO] Some tests failed"
fi

exit $EXIT_CODE
