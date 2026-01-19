# ? RISC-V-TLM: WSL Complete Setup Summary

## ?? What Has Been Created

You now have a complete WSL-based build and test environment for RISC-V-TLM!

## ?? New Files Created

### 1. **wsl_complete_build_and_test.sh**
   - **Purpose**: Complete automated build and test suite
   - **Features**:
     - Auto-installs dependencies (RISC-V toolchain, build tools)
     - Builds both pipelined and non-pipelined configurations
     - Compiles all test programs to hex format
     - Runs comprehensive test suite
     - Generates detailed reports

### 2. **wsl_quick_test.sh**
   - **Purpose**: Fast verification test runner
   - **Features**:
     - Runs only minimal and fast tests
     - Quick feedback (30-60 seconds)
     - Perfect for development iteration

### 3. **wsl_build.bat**
   - **Purpose**: Windows launcher for WSL scripts
   - **Usage**: Double-click or run from Command Prompt
   - **Features**: Automatically converts paths and runs WSL

### 4. **WSL_BUILD_GUIDE.md**
   - **Purpose**: Complete documentation
   - **Contents**:
     - Installation instructions
     - Usage guide
     - Troubleshooting
     - Peripheral memory map
     - Examples

### 5. **tests/full_system/robust_minimal.c**
   - **Purpose**: Quick sanity test
   - **Runtime**: ~1 second
   - **Tests**: Basic computation and trace output

### 6. **src/Debug.cpp** (Updated)
   - **Purpose**: Fixed for Linux/WSL
   - **Features**:
     - Full GDB remote stub on Linux
     - No-op stubs on Windows
     - Proper platform detection

## ?? How to Use

### Option 1: Automated (Recommended)

From Windows:
```batch
wsl_build.bat
```

From WSL:
```bash
chmod +x wsl_complete_build_and_test.sh
./wsl_complete_build_and_test.sh
```

### Option 2: Quick Test Only

```bash
chmod +x wsl_quick_test.sh
./wsl_quick_test.sh
```

### Option 3: Manual Commands

```bash
# Enter WSL
wsl

# Build
mkdir -p build_plain
cmake -S . -B build_plain -DENABLE_PIPELINED_ISS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build_plain --parallel

# Run
./build_plain/RISCV_VP -f tests/hex/robust_fast32.hex -R 32
```

## ?? What Gets Tested

### Test Levels

| Test | Runtime | What It Tests |
|------|---------|---------------|
| **robust_minimal** | 1 sec | Basic ALU, trace output |
| **robust_fast** | 10 sec | Timer IRQ, DMA, atomics, M extension |
| **robust_system_test** | 2 min | Full system workout, all peripherals |

### Configurations

| Config | Description | Performance |
|--------|-------------|-------------|
| **Plain** | Sequential execution | Baseline |
| **Pipelined** | Pipelined ISS | +15-40% IPC |

### Architectures

- **RV32IMAC**: 32-bit RISC-V with Integer, Multiply, Atomic, Compressed
- **RV64IMAC**: 64-bit RISC-V with same extensions

## ? Expected Results

After running `wsl_complete_build_and_test.sh`, you should see:

```
=== RISC-V-TLM Complete WSL Build and Test ===

Step 1: Checking dependencies...
? gcc found
? g++ found
? cmake found
...

Step 2: Building RISC-V-TLM...
Building Plain (non-pipelined) configuration...
? Plain build complete
Building Pipelined configuration...
? Pipelined build complete

Step 3: Compiling test programs to hex format...
Compiling tests/full_system/robust_system_test.c for rv32imac...
? Created tests/hex/robust_system_test32.hex
...

Step 4: Running tests...

Running: RV32_Fast_Plain
[ROBUST] start
[ROBUST] accum=...
[ROBUST] end
? Test completed successfully

...

=== Test Summary ===
Passed: 8
Failed: 0

? All tests passed!
```

## ?? Common Issues and Fixes

### Issue: "WSL not found"
**Fix**: Install WSL
```powershell
wsl --install
```

### Issue: "Permission denied"
**Fix**: Make scripts executable
```bash
chmod +x wsl_*.sh
```

### Issue: "RISC-V toolchain not found"
**Fix**: The script auto-installs it, but if it fails:
```bash
sudo apt update
sudo apt install gcc-riscv64-unknown-elf
```

### Issue: "Build fails"
**Fix**: Clean and rebuild
```bash
rm -rf build_plain build_pipe
./wsl_complete_build_and_test.sh
```

## ?? Performance Metrics

The test suite reports:

1. **Cycles**: Total simulation cycles
2. **Instructions**: Total instructions executed
3. **IPC**: Instructions per cycle (higher is better)
4. **IPS**: Instructions per second (simulation speed)
5. **Speedup**: Pipeline vs non-pipeline improvement

Example output:
```
Pipeline vs Non-Pipeline (instr/sec)
TLM pipelined     : 2,450,000
TLM non-pipelined : 1,850,000
VP pipe=on        : 2,380,000
VP pipe=off       : 1,820,000

Speedup (pipeline vs non):
TLM: +32.43%
VP : +30.77%
```

## ?? Next Steps

1. **Verify Installation**
   ```bash
   ./wsl_quick_test.sh
   ```

2. **Run Full Test Suite**
   ```bash
   ./wsl_complete_build_and_test.sh
   ```

3. **Try GDB Debugging**
   ```bash
   # Terminal 1
   ./build_plain/RISCV_VP -f tests/hex/robust_fast32.hex -R 32 -D
   
   # Terminal 2
   riscv32-unknown-elf-gdb
   (gdb) target remote localhost:1234
   ```

4. **Write Custom Tests**
   - Use `tests/full_system/robust_minimal.c` as template
   - Compile with provided commands in WSL_BUILD_GUIDE.md

5. **Benchmark Performance**
   ```bash
   ./wsl_run.sh compare tests/hex/robust_fast32.hex 200000
   ```

## ?? Documentation Files

- **WSL_BUILD_GUIDE.md**: Comprehensive usage guide
- **README.md**: Project overview (existing)
- **CMakeLists.txt**: Build configuration
- **scripts/**: Additional utilities

## ??? Development Workflow

```bash
# 1. Make code changes
vim src/CPU.cpp

# 2. Quick test
./wsl_quick_test.sh

# 3. Full test before commit
./wsl_complete_build_and_test.sh

# 4. Commit
git add .
git commit -m "Your changes"
git push
```

## ?? Success Indicators

You'll know everything is working when:

- ? All dependencies install without errors
- ? Both builds complete successfully
- ? All hex files are generated
- ? Tests show "[ROBUST] end" messages
- ? No DMA mismatches reported
- ? Performance metrics are displayed

## ?? Tips

1. **Use quick_test during development** - Much faster iteration
2. **Check logs in /tmp/test_*.log** - Detailed output saved here
3. **WSL paths start with /mnt/c/** - Your C: drive in WSL
4. **Use wsl_build.bat from Windows** - No need to enter WSL manually
5. **Build directories are cached** - Incremental builds are fast

## ?? Getting Help

If you encounter issues:

1. Check WSL_BUILD_GUIDE.md troubleshooting section
2. Look at build logs: `./wsl_complete_build_and_test.sh 2>&1 | tee build.log`
3. Verify WSL version: `wsl --version`
4. Check Ubuntu version: `lsb_release -a`
5. Test RISC-V toolchain: `riscv32-unknown-elf-gcc --version`

## ?? Summary

You now have:
- ? Complete WSL build environment
- ? Automated test suite
- ? Quick verification tools
- ? Comprehensive documentation
- ? GDB debugging support
- ? Performance benchmarking
- ? Example test programs

**Ready to go! Run `wsl_build.bat` to get started! ??**
