#!/bin/bash
# Build RISC-V-TLM project using Ninja only
# Usage: ./wsl_build_ninja.sh [clean] [debug]

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR="$ROOT_DIR/build"
BUILD_TYPE="Release"
CLEAN_BUILD=0

# Parse arguments
while [[ $# -gt 0 ]]; do
  case "$1" in
    clean)
      CLEAN_BUILD=1
      shift
      ;;
    debug)
      BUILD_TYPE="Debug"
      shift
      ;;
    *)
      echo "Unknown argument: $1"
      echo "Usage: $0 [clean] [debug]"
      exit 1
      ;;
  esac
done

echo "RISC-V TLM Ninja Build Script"
echo "=============================="

# Install Ninja if not available
if ! command -v ninja >/dev/null 2>&1; then
  echo "Installing ninja-build..."
  sudo apt update && sudo apt install -y ninja-build
  echo "? Ninja installed"
fi

# Clean build if requested
if [[ $CLEAN_BUILD -eq 1 ]]; then
  echo "Cleaning build directory..."
  rm -rf "$BUILD_DIR"
  echo "? Build directory cleaned"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with Ninja
echo "Configuring with Ninja (${BUILD_TYPE})..."
cmake .. -G "Ninja" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "? Configuration complete"

# Build with Ninja
echo "Building with Ninja..."
ninja -j"$(nproc)"

echo "? Build complete"

# Show build artifacts
echo ""
echo "Build artifacts:"
ls -la RISCV_VP RISCV_TLM libriscv_tlm_core.a 2>/dev/null || echo "  (Some artifacts may be missing)"

echo ""
echo "To run tests:"
echo "  ctest -V"
echo ""
echo "To run executables:"
echo "  ./RISCV_VP -f ../tests/hex/simple.hex --max-instr 200000"
echo "  ./RISCV_TLM -f ../tests/hex/simple.hex --max-instr 50000"