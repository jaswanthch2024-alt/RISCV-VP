#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Complete WSL migration: copy project, clean up Windows builds, build everything
# Usage: Run from Windows: wsl bash migrate_to_wsl.sh

set -euo pipefail

echo "=========================================="
echo "RISC-V TLM Complete WSL Migration"
echo "=========================================="

# Detect source path (Windows mount)
SRC_WIN="/mnt/c/Users/jaswa/Source/Repos/RISC-V-TLM"
DST_WSL="$HOME/RISC-V-TLM"

if [[ ! -d "$SRC_WIN" ]]; then
  echo "[ERROR] Source not found: $SRC_WIN" >&2
  exit 1
fi

echo "[1/8] Checking WSL environment"
if [[ "$(uname -s)" != Linux* ]]; then
  echo "[ERROR] Must run inside WSL" >&2
  exit 1
fi

echo "[2/8] Installing required packages"
sudo apt update
sudo apt install -y build-essential cmake ninja-build dos2unix git rsync

# Clean Windows build artifacts so nothing runs from Windows mount later
echo "[3/8] Cleaning Windows-side build artifacts (build, build-vs)"
rm -rf "$SRC_WIN/build" "$SRC_WIN/build-vs" 2>/dev/null || true

# Copy minimal project to WSL native FS (exclude heavy/Windows-only dirs)
echo "[4/8] Copying project to WSL native filesystem (ext4)"
echo "      From: $SRC_WIN"
echo "      To:   $DST_WSL"
if [[ -d "$DST_WSL" ]]; then
  echo "      (Removing existing copy)"
  rm -rf "$DST_WSL"
fi
rsync -a --delete \
  --exclude='build/' \
  --exclude='build-vs/' \
  --exclude='vcpkg/' \
  --exclude='.vs/' \
  "$SRC_WIN/" "$DST_WSL/" || echo "[WARN] rsync reported non-fatal issues; continuing"

cd "$DST_WSL"
echo "      Working directory: $(pwd)"

echo "[5/8] Converting line endings (CRLF -> LF) and making scripts executable"
find . -type f \( -name '*.sh' -o -name '*.bash' -o -name '*.py' \) -exec dos2unix {} + 2>/dev/null || true
find . -name '*.sh' -exec chmod +x {} +

echo "[6/8] Checking SystemC installation"
if ! ldconfig -p | grep -q libsystemc; then
  echo "[INFO] SystemC not found. Installing SystemC 2.3.4..."
  SYSTEMC_TAR="systemc-2.3.4.tar.gz"
  if [[ ! -f "/tmp/$SYSTEMC_TAR" ]]; then
    wget -q -P /tmp https://accellera.org/images/downloads/standards/systemc/systemc-2.3.4.tar.gz
  fi
  tar -C /tmp -xf "/tmp/$SYSTEMC_TAR"
  pushd /tmp/systemc-2.3.4 >/dev/null
  mkdir -p build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=/usr/local
  make -j$(nproc)
  sudo make install
  sudo ldconfig
  popd >/dev/null
  echo "[INFO] SystemC installed successfully"
else
  echo "[INFO] SystemC already installed"
fi

echo "[7/8] Building project (RISCV_TLM + RISCV_VP) in WSL home"
rm -rf build
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

echo "[8/8] Running tests in WSL home"
ctest --test-dir build --output-on-failure -j2

echo "[SUCCESS] WSL build and tests completed in $DST_WSL"

echo ""
echo "=========================================="
echo "Migration Complete!"
echo "=========================================="
echo "Project location: $DST_WSL"
echo ""
echo "Next steps (in WSL):"
echo "  cd ~/RISC-V-TLM"
echo "  ./wsl_run_all_tests.sh         # Run all tests"
echo "  ./build/RISCV_VP -f tests/hex/simple.hex --max-instr 200000"
echo ""
echo "To clean up Windows side (OPTIONAL, from PowerShell):"
echo "  Remove-Item -Recurse -Force C:\\Users\\jaswa\\Source\\Repos\\RISC-V-TLM\\build"
echo "  Remove-Item -Recurse -Force C:\\Users\\jaswa\\Source\\Repos\\RISC-V-TLM\\build-vs"
echo ""
