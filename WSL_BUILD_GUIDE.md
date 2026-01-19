# WSL Build and Test Guide for RISCV-VP

This guide explains how to build and test the RISCV-VP project using WSL (Windows Subsystem for Linux) on Windows.

## Why WSL?

WSL provides a complete Linux environment on Windows, which allows you to:
- Use the RISC-V GNU toolchain natively
- Build with proper GCC/G++ compilers
- Run Linux-specific features like the GDB remote stub
- Get better build performance
- Use standard Linux development tools

## Quick Start

###  1. Install WSL (if not already installed)

```powershell
# Run in PowerShell as Administrator
wsl --install
```

Restart your computer when prompted.

### 2. Build and Test Everything

```batch
# From Windows Command Prompt or PowerShell, in the project directory:
wsl_build.bat
```

That's it! The script will:
- ? Check and install all dependencies
- ? Build both pipelined and non-pipelined configurations
- ? Compile test programs to hex format
- ? Run comprehensive tests
- ? Display results

## Manual WSL Commands

If you prefer to run commands manually in WSL:

```bash
# Enter WSL
wsl

# Navigate to project directory
cd /mnt/c/Users/YourUsername/source/repos/RISCV-VP

# Make script executable
chmod +x wsl_complete_build_and_test.sh

# Run full build and test
./wsl_complete_build_and_test.sh
```

## What Gets Built

### Configurations
1. **Plain (Non-Pipelined)** - `build_plain/`
   - Traditional sequential instruction execution
   - Simpler, easier to debug
   - RISCV_TLM and RISCV_VP executables

2. **Pipelined** - `build_pipe/`
   - Pipelined instruction execution
   - Higher performance
   - RISCV_TLM and RISCV_VP executables

### Test Programs

All test programs are compiled for both RV32 and RV64:

1. **robust_minimal** - Quick sanity test (~1 second)
   - Basic computation and trace output
   - Good for verifying the setup works

2. **robust_fast** - Medium comprehensive test (~10 seconds)
   - Timer interrupts, DMA, atomics
   - M extension (multiply/divide)
   - Reduced workload for faster execution

3. **robust_system_test** - Full system test (~2 minutes)
   - Complete system exercising
   - All peripherals and extensions
   - Performance counters and verification

## Running Individual Tests

After building, you can run tests manually:

```bash
# RV32 tests
./build_plain/RISCV_VP -f tests/hex/robust_fast32.hex -R 32
./build_pipe/RISCV_VP -f tests/hex/robust_system_test32.hex -R 32

# RV64 tests
./build_plain/RISCV_VP -f tests/hex/robust_fast64.hex -R 64
./build_pipe/RISCV_VP -f tests/hex/robust_system_test64.hex -R 64
```

## Debugging with GDB

The Linux build includes GDB remote stub support:

```bash
# Terminal 1: Start simulator with debugging
./build_plain/RISCV_VP -f tests/hex/robust_fast32.hex -R 32 -D

# Terminal 2: Connect with GDB
riscv32-unknown-elf-gdb tests/hex/robust_fast32.elf
(gdb) target remote localhost:1234
(gdb) break main
(gdb) continue
```

## Compiling Your Own Programs

```bash
# RV32
riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -O2 -nostdlib \
    -Wl,--entry=main -Wl,--gc-sections \
    your_program.c -o your_program.elf

riscv32-unknown-elf-objcopy -O ihex your_program.elf your_program.hex

# RV64
riscv64-unknown-elf-gcc -march=rv64imac -mabi=lp64 -O2 -nostdlib \
    -Wl,--entry=main -Wl,--gc-sections \
    your_program.c -o your_program.elf

riscv64-unknown-elf-objcopy -O ihex your_program.elf your_program.hex
```

## Peripheral Memory Map

When writing test programs, use these addresses:

| Peripheral | Base Address  | Description |
|------------|---------------|-------------|
| Trace      | 0x40000000    | Character output (UART-like) |
| Timer      | 0x40004000    | MTIME, MTIMECMP registers |
| DMA        | 0x30000000    | DMA controller |
| CLINT      | 0x02000000    | Core-Local Interruptor |
| PLIC       | 0x0C000000    | Platform-Level Interrupt Controller |

## Troubleshooting

### "wsl command not found"
WSL is not installed. Run `wsl --install` in PowerShell as Administrator.

### "riscv32-unknown-elf-gcc not found"
The script should install this automatically. If it fails:
```bash
sudo apt update
sudo apt install gcc-riscv64-unknown-elf
```

### "SystemC not found"
The project uses a bundled SystemC. Ensure `systemc/` directory exists in the project root.

### Build fails with "CMake cache mismatch"
The script automatically cleans mismatched caches. If issues persist:
```bash
rm -rf build_plain build_pipe
./wsl_complete_build_and_test.sh
```

### Tests timeout or hang
Reduce the workload by using `robust_fast` instead of `robust_system_test`, or modify the loop counts in the test files.

## Performance Comparison

The test script automatically runs all configurations and reports:
- Instructions per second (IPS)
- Total cycles
- Instructions per cycle (IPC)
- Pipeline speedup percentage

Expected pipeline improvement: **15-40% higher IPC** depending on the workload.

## File Locations

```
RISCV-VP/
??? wsl_build.bat                   # Windows launcher
??? wsl_complete_build_and_test.sh  # Main WSL build script
??? build_plain/                    # Non-pipelined build
?   ??? RISCV_TLM
?   ??? RISCV_VP
??? build_pipe/                     # Pipelined build
?   ??? RISCV_TLM
?   ??? RISCV_VP
??? tests/
?   ??? full_system/
?   ?   ??? robust_minimal.c
?   ?   ??? robust_fast.c
?   ?   ??? robust_system_test.c
?   ??? hex/                        # Compiled hex files
?       ??? robust_minimal32.hex
?       ??? robust_fast32.hex
?       ??? robust_system_test32.hex
?       ??? robust_minimal64.hex
?       ??? robust_fast64.hex
?       ??? robust_system_test64.hex
??? src/                            # Source code
```

## Next Steps

1. ? Run `wsl_build.bat` to verify everything works
2. ? Examine test output in `/tmp/test_*.log`
3. ? Try running tests individually to understand each one
4. ? Write your own test programs using the examples as templates
5. ? Use GDB debugging to step through code execution

## Support

- Check log files in `/tmp/test_*.log` for detailed test output
- Build logs are displayed during compilation
- Use `./wsl_complete_build_and_test.sh 2>&1 | tee build.log` to save full output

## Additional Resources

- RISC-V ISA Specification: https://riscv.org/technical/specifications/
- SystemC Documentation: https://www.accellera.org/downloads/standards/systemc
- TLM-2.0 Reference: https://www.accellera.org/downloads/standards/systemc
