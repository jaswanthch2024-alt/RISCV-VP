#!/bin/bash
# Script to set up and build RISC-V-TLM in WSL

set -e  # Exit on error

# Install required packages
echo "Installing required packages..."
sudo apt update
sudo apt install -y build-essential cmake ninja-build g++ libboost-dev doxygen git gdb net-tools wget

# Install SystemC (required with C++17 support)
echo "Installing SystemC..."
SYSTEMC_VERSION="2.3.3"
SYSTEMC_DIR="/usr/local/systemc-$SYSTEMC_VERSION"

# Only download and install if not already installed
if [ ! -d "$SYSTEMC_DIR" ]; then
    cd /tmp
    wget -O "systemc-$SYSTEMC_VERSION.tar.gz" "https://www.accellera.org/images/downloads/standards/systemc/systemc-$SYSTEMC_VERSION.tar.gz"
    tar -xzf "systemc-$SYSTEMC_VERSION.tar.gz"
    cd "systemc-$SYSTEMC_VERSION"
    
    # Create build directory and configure with C++17 support
    mkdir -p build
    cd build
    cmake .. -G Ninja -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=$SYSTEMC_DIR
    ninja -j"$(nproc)"
    sudo ninja install
fi

# Install spdlog
echo "Installing spdlog..."
if ! dpkg -l | grep -q libspdlog-dev; then
    sudo apt install -y libspdlog-dev
fi

# Set environment variables
echo "Setting up environment variables..."
export SYSTEMC_HOME=$SYSTEMC_DIR
export LD_LIBRARY_PATH=$SYSTEMC_DIR/lib-linux64:$LD_LIBRARY_PATH

# Add environment variables to .bashrc if they're not already there
if ! grep -q "SYSTEMC_HOME=$SYSTEMC_DIR" ~/.bashrc; then
    echo "export SYSTEMC_HOME=$SYSTEMC_DIR" >> ~/.bashrc
    echo "export LD_LIBRARY_PATH=\$SYSTEMC_HOME/lib-linux64:\$LD_LIBRARY_PATH" >> ~/.bashrc
    echo "Environment variables added to .bashrc"
fi

# Create build directory and build the project
echo "Building the RISC-V-TLM project..."
mkdir -p build
cd build
cmake .. -G Ninja -DCMAKE_CXX_STANDARD=17
cmake --build . -- -j"$(nproc)"

echo "Build complete. You can run the simulator with:"
echo "./RISCV_VP -f <path_to_hex_file>"
echo
echo "For GDB debugging, run:"
echo "./RISCV_VP -f <path_to_hex_file> -D"
echo "Then in another terminal, connect with: gdb -ex 'target remote localhost:1234' <your_elf_file>"