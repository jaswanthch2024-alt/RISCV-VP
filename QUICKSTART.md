# ?? RISCV-VP Quick Start Guide (WSL on Windows)

## ? Super Quick Start (3 commands)

```powershell
# 1. Open PowerShell in project directory
cd C:\Users\YourName\source\repos\RISCV-VP

# 2. Run setup verification
.\wsl_build.ps1 -Setup

# 3. Build and test everything
.\wsl_build.ps1
```

Done! ??

---

## ?? What You Need

? Windows 10/11  
? WSL installed (`wsl --install` if not)  
? This project directory  

That's it! Everything else is auto-installed.

---

## ??? Available Commands

### From Windows PowerShell:

```powershell
# Verify setup
.\wsl_build.ps1 -Setup

# Quick test (fast, 1-2 min)
.\wsl_build.ps1 -Quick

# Full build and test (comprehensive, 5-10 min)
.\wsl_build.ps1

# Or use the batch file
wsl_build.bat
```

### From WSL/Linux:

```bash
# Verify environment
./wsl_setup.sh

# Quick test
./wsl_quick_test.sh

# Full build and test
./wsl_complete_build_and_test.sh
```

---

## ?? What Gets Built

### Executables Created:
- `build_plain/RISCV_TLM` - Traditional simulator (non-pipelined)
- `build_plain/RISCV_VP` - Virtual Platform (non-pipelined)
- `build_pipe/RISCV_TLM` - Traditional simulator (pipelined)
- `build_pipe/RISCV_VP` - Virtual Platform (pipelined)

### Test Programs Compiled:
- `tests/hex/robust_minimal32.hex` - Quick sanity test (RV32)
- `tests/hex/robust_fast32.hex` - Fast comprehensive test (RV32)
- `tests/hex/robust_system_test32.hex` - Full system test (RV32)
- `tests/hex/robust_minimal64.hex` - Quick sanity test (RV64)
- `tests/hex/robust_fast64.hex` - Fast comprehensive test (RV64)
- `tests/hex/robust_system_test64.hex` - Full system test (RV64)

---

## ? Success Indicators

You'll know it worked when you see:

```
=== Test Summary ===
Passed: 8
Failed: 0

? All tests passed!
```

---

## ?? Understanding the Output

### Build Phase:
```
Building Plain (non-pipelined) configuration...
? Plain build complete
Building Pipelined configuration...
? Pipelined build complete
```

### Test Phase:
```
Running: RV32_Fast_Plain
[ROBUST] start
[ROBUST] accum=...
[ROBUST] atomic_count=...
[ROBUST] dma_done=...
[ROBUST] irq_count=...
[ROBUST] end
? Test completed successfully
```

### Performance Metrics:
```
RV32 VP Pipeline IPC improvement: 32.45%
```

---

## ?? Manual Testing

After building, run tests manually:

### From WSL:
```bash
# RV32 tests
./build_plain/RISCV_VP -f tests/hex/robust_fast32.hex -R 32
./build_pipe/RISCV_VP -f tests/hex/robust_fast32.hex -R 32

# RV64 tests
./build_plain/RISCV_VP -f tests/hex/robust_fast64.hex -R 64
./build_pipe/RISCV_VP -f tests/hex/robust_fast64.hex -R 64
```

### Command-line Options:
```
-f <hex_file>   : Hex file to load
-R <32|64>      : Architecture (32 or 64 bit)
-D              : Enable GDB remote debugging
-M <num>        : Max instructions to simulate
```

---

## ?? Debugging with GDB

```bash
# Terminal 1: Start simulator with debug
./build_plain/RISCV_VP -f tests/hex/robust_fast32.hex -R 32 -D

# Terminal 2: Connect GDB
riscv32-unknown-elf-gdb
(gdb) target remote localhost:1234
(gdb) break main
(gdb) continue
```

---

## ?? Writing Your Own Tests

### 1. Create C file:
```c
// my_test.c
#define TRACE (*(volatile unsigned char*)0x40000000)

void putch(char c) { TRACE = c; }

int main() {
    const char *msg = "Hello RISC-V!\n";
    while (*msg) putch(*msg++);
    __asm__ volatile("ecall");  // End simulation
    return 0;
}
```

### 2. Compile to hex:
```bash
# RV32
riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -O2 -nostdlib \
    -Wl,--entry=main my_test.c -o my_test.elf
riscv32-unknown-elf-objcopy -O ihex my_test.elf my_test.hex

# RV64
riscv64-unknown-elf-gcc -march=rv64imac -mabi=lp64 -O2 -nostdlib \
    -Wl,--entry=main my_test.c -o my_test.elf
riscv64-unknown-elf-objcopy -O ihex my_test.elf my_test.hex
```

### 3. Run:
```bash
./build_plain/RISCV_VP -f my_test.hex -R 32
```

---

## ??? Peripheral Memory Map

Use these addresses in your test programs:

| Peripheral | Address | Usage |
|------------|---------|-------|
| **Trace** | 0x40000000 | Character output (like UART) |
| **Timer Low** | 0x40004000 | MTIME lower 32 bits |
| **Timer High** | 0x40004004 | MTIME upper 32 bits |
| **Timer Cmp Low** | 0x40004008 | MTIMECMP lower 32 bits |
| **Timer Cmp High** | 0x4000400C | MTIMECMP upper 32 bits |
| **DMA** | 0x30000000 | DMA controller base |

### Example - Output Character:
```c
#define TRACE (*(volatile unsigned char*)0x40000000)
TRACE = 'A';  // Outputs 'A'
```

### Example - Read Timer:
```c
#define MTIME_LO (*(volatile unsigned int*)0x40004000)
#define MTIME_HI (*(volatile unsigned int*)0x40004004)

unsigned long long time = ((unsigned long long)MTIME_HI << 32) | MTIME_LO;
```

---

## ?? Performance Comparison

### Expected Results:

| Configuration | IPC | Relative Speed |
|---------------|-----|----------------|
| Plain (non-pipelined) | 1.00 | Baseline |
| Pipelined | 1.30-1.40 | +30-40% |

### Factors Affecting Performance:
- Branch prediction hits
- Data dependencies
- Memory access patterns
- Instruction mix

---

## ? Troubleshooting

### "WSL command not found"
```powershell
# Install WSL (run as Administrator)
wsl --install
# Restart computer
```

### "Permission denied" errors
```bash
chmod +x *.sh
```

### "Build failed" or "CMake errors"
```bash
# Clean everything
rm -rf build build_plain build_pipe
# Try again
./wsl_complete_build_and_test.sh
```

### "RISC-V compiler not found"
The script should auto-install it. If not:
```bash
sudo apt update
sudo apt install gcc-riscv64-unknown-elf
```

### Tests hang or timeout
Use faster tests:
```bash
./wsl_quick_test.sh
```

---

## ?? Documentation

- **WSL_BUILD_GUIDE.md** - Comprehensive guide
- **WSL_SETUP_COMPLETE.md** - Detailed setup summary
- **README.md** - Project overview
- **QUICKSTART.md** - This file!

---

## ?? Development Workflow

```bash
# 1. Edit code
vim src/CPU.cpp

# 2. Quick test
./wsl_quick_test.sh

# 3. If OK, full test
./wsl_complete_build_and_test.sh

# 4. Commit
git add .
git commit -m "Your changes"
git push
```

---

## ?? Pro Tips

1. **Use quick tests during development** - Saves time
2. **Full tests before commits** - Ensures quality
3. **Check /tmp/test_*.log for details** - When debugging
4. **Use -D flag for GDB debugging** - Step through code
5. **WSL paths are /mnt/c/...** - C: drive in WSL

---

## ?? Need Help?

1. Check **WSL_BUILD_GUIDE.md** for detailed troubleshooting
2. Look at error messages in the output
3. Verify with `./wsl_setup.sh`
4. Check logs: `cat /tmp/test_*.log`
5. Test individually: `./build_plain/RISCV_VP -f tests/hex/robust_minimal32.hex -R 32`

---

## ? That's It!

You're ready to build and test RISCV-VP on WSL!

**Start with:** `.\wsl_build.ps1 -Setup`

?? **Happy Simulating!** ??
