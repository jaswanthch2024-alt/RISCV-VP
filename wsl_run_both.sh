#!/bin/bash
# Run both RISCV_VP and RISCV_TLM in WSL, side-by-side using tmux, with separate logs
# Usage: ./wsl_run_both.sh [-f tests/hex/simple.hex] [--no-attach] [-- extra_args]

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR="$ROOT_DIR/build"
LOG_DIR="$ROOT_DIR/logs"
HEX_FILE="tests/hex/simple.hex"
EXTRA_ARGS=()
ATTACH=1

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    -f|--file)
      HEX_FILE="$2"; shift 2;;
    --no-attach)
      ATTACH=0; shift;;
    --)
      shift; EXTRA_ARGS=("$@"); break;;
    *)
      # Pass-through unknown args to executables
      EXTRA_ARGS+=("$1"); shift;;
  esac
done

# Detect SystemC and set runtime libs if available (same logic as wsl_build.sh)
SYSTEMC_VERSION="2.3.3"
THIRD_PARTY_DIR="$ROOT_DIR/third_party"
SYSTEMC_PREFIX="$THIRD_PARTY_DIR/systemc-$SYSTEMC_VERSION"
if [ -z "${SYSTEMC_HOME:-}" ]; then
  if [ -d "$SYSTEMC_PREFIX" ]; then
    export SYSTEMC_HOME="$SYSTEMC_PREFIX"
  elif [ -d "/usr/local/systemc-$SYSTEMC_VERSION" ]; then
    export SYSTEMC_HOME="/usr/local/systemc-$SYSTEMC_VERSION"
  fi
fi
if [ -n "${SYSTEMC_HOME:-}" ]; then
  export LD_LIBRARY_PATH="$SYSTEMC_HOME/lib-linux64:$SYSTEMC_HOME/lib:${LD_LIBRARY_PATH-}"
fi

# Ensure Ninja is available
if ! command -v ninja >/dev/null 2>&1; then
  echo "Installing ninja-build..."
  sudo apt update && sudo apt install -y ninja-build
fi

# Ensure build exists and binaries are built
if [ ! -d "$BUILD_DIR" ]; then
  echo "Build directory not found. Configuring with Ninja..."
  mkdir -p "$BUILD_DIR"
  pushd "$BUILD_DIR" >/dev/null
  cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17
  popd >/dev/null
fi

pushd "$BUILD_DIR" >/dev/null
if [ ! -x ./RISCV_VP ] || [ ! -x ./RISCV_TLM ]; then
  echo "Building project with Ninja..."
  ninja -j"$(nproc)"
fi
popd >/dev/null

# Validate hex file
if [ ! -f "$ROOT_DIR/$HEX_FILE" ]; then
  echo "Error: hex file '$HEX_FILE' not found relative to repo root ($ROOT_DIR)" >&2
  exit 1
fi

mkdir -p "$LOG_DIR"
VP_LOG="$LOG_DIR/riscv_vp.log"
TLM_LOG="$LOG_DIR/riscv_tlm.log"

# Prefer tmux for split view
if command -v tmux >/dev/null 2>&1; then
  SESSION_NAME="riscv_run"
  # Kill any existing session with same name
  tmux has-session -t "$SESSION_NAME" 2>/dev/null && tmux kill-session -t "$SESSION_NAME"

  # Start VP in first pane
  tmux new-session -d -s "$SESSION_NAME" "cd '$BUILD_DIR' && stdbuf -oL -eL ./RISCV_VP -f '$HEX_FILE' ${EXTRA_ARGS[*]} 2>&1 | tee '$VP_LOG'"
  # Split and start TLM in second pane
  tmux split-window -h "cd '$BUILD_DIR' && stdbuf -oL -eL ./RISCV_TLM -f '$HEX_FILE' ${EXTRA_ARGS[*]} 2>&1 | tee '$TLM_LOG'"
  tmux select-layout even-horizontal >/dev/null

  echo "Launched tmux session '$SESSION_NAME' with two panes (VP left, TLM right)."
  echo "Logs:"
  echo "  VP : $VP_LOG"
  echo "  TLM: $TLM_LOG"
  if [[ "$ATTACH" -eq 1 ]]; then
    tmux attach -t "$SESSION_NAME"
  else
    echo "Not attaching (--no-attach). Attach manually with: tmux attach -t $SESSION_NAME"
  fi
else
  echo "tmux not found. Falling back to background processes with log files." >&2
  echo "Install tmux with: sudo apt update && sudo apt install -y tmux" >&2
  (
    cd "$BUILD_DIR"
    nohup bash -lc "./RISCV_VP -f '$HEX_FILE' ${EXTRA_ARGS[*]} 2>&1 | tee '$VP_LOG'" >/dev/null 2>&1 & echo $! > "$LOG_DIR/vp.pid"
    nohup bash -lc "./RISCV_TLM -f '$HEX_FILE' ${EXTRA_ARGS[*]} 2>&1 | tee '$TLM_LOG'" >/dev/null 2>&1 & echo $! > "$LOG_DIR/tlm.pid"
  )
  echo "Started both processes in background. Tail logs in two terminals:"
  echo "  tail -f '$VP_LOG'"
  echo "  tail -f '$TLM_LOG'"
fi
