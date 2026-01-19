#!/bin/bash
# Script to build RISC-V-TLM in WSL/Linux (builds SystemC locally if needed)
# Usage: ./wsl_build.sh [--debug|--release] [--strict] [--no-tests]

set -euo pipefail

BUILD_TYPE=Release
STRICT_FLAG=OFF
BUILD_TESTING=ON

while [[ $# -gt 0 ]]; do
  case $1 in
    --debug) BUILD_TYPE=Debug; shift;;
    --release) BUILD_TYPE=Release; shift;;
    --strict) STRICT_FLAG=ON; shift;;
    --no-tests) BUILD_TESTING=OFF; shift;;
    *) echo "Unknown option: $1"; exit 1;;
  esac
done

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
THIRD_PARTY_DIR="$ROOT_DIR/third_party"
SYSTEMC_VERSION="2.3.3"
SYSTEMC_PREFIX="$THIRD_PARTY_DIR/systemc-$SYSTEMC_VERSION"

mkdir -p "$THIRD_PARTY_DIR"

# Detect SystemC
if [ -z "${SYSTEMC_HOME:-}" ]; then
  if [ -d "$SYSTEMC_PREFIX" ]; then
    export SYSTEMC_HOME="$SYSTEMC_PREFIX"
  elif [ -d "/usr/local/systemc-$SYSTEMC_VERSION" ]; then
    export SYSTEMC_HOME="/usr/local/systemc-$SYSTEMC_VERSION"
  fi
fi

# Build SystemC locally if not found
if [ -z "${SYSTEMC_HOME:-}" ] || [ ! -d "$SYSTEMC_HOME/include" ]; then
  echo "SystemC not found. Bootstrapping SystemC $SYSTEMC_VERSION under $THIRD_PARTY_DIR ..."
  pushd "$THIRD_PARTY_DIR" >/dev/null
  if [ ! -d "systemc-$SYSTEMC_VERSION" ]; then
    curl -LO https://www.accellera.org/images/downloads/standards/systemc/systemc-$SYSTEMC_VERSION.tar.gz
    tar xf systemc-$SYSTEMC_VERSION.tar.gz
  fi
  pushd "systemc-$SYSTEMC_VERSION" >/dev/null
  mkdir -p build && cd build
  cmake .. -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX="$(pwd)/.."
  cmake --build . -- -j"$(nproc)"
  cmake --install .
  popd >/dev/null
  popd >/dev/null
  export SYSTEMC_HOME="$SYSTEMC_PREFIX"
fi

# Ensure runtime libs are found
if [ -n "${SYSTEMC_HOME:-}" ]; then
  export LD_LIBRARY_PATH="$SYSTEMC_HOME/lib-linux64:$SYSTEMC_HOME/lib:${LD_LIBRARY_PATH-}"
fi

# Configure and build project
BUILD_DIR="$ROOT_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
if [ ! -f build.ninja ]; then
  cmake .. -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_CXX_STANDARD=17 -DENABLE_STRICT=$STRICT_FLAG -DBUILD_TESTING=$BUILD_TESTING
else
  cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DENABLE_STRICT=$STRICT_FLAG -DBUILD_TESTING=$BUILD_TESTING .
fi
cmake --build . -- -j"$(nproc)"

# Run tests if enabled
if [ "$BUILD_TESTING" = "ON" ]; then
  echo "Running tests..."
  ctest --output-on-failure
fi

# Print where binaries ended up
echo "Build complete! Binaries:"
ls -l "$BUILD_DIR"/RISCV_TLM || true
ls -l "$BUILD_DIR"/RISCV_VP || true