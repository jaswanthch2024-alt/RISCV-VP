# ?? RISC-V-TLM WSL Build System - Complete Documentation Index

## ?? Start Here

### New Users:
1. **[QUICKSTART.md](QUICKSTART.md)** - Get started in 3 commands
2. **[WSL_BUILD_GUIDE.md](WSL_BUILD_GUIDE.md)** - Complete usage guide
3. Run: `.\wsl_build.ps1 -Setup` (Windows) or `./wsl_setup.sh` (WSL)

### Developers:
1. **[WSL_IMPLEMENTATION_SUMMARY.md](WSL_IMPLEMENTATION_SUMMARY.md)** - Technical details
2. **[WSL_SETUP_COMPLETE.md](WSL_SETUP_COMPLETE.md)** - Implementation guide
3. Run: `./wsl_quick_test.sh` for fast iteration

---

## ?? Documentation Files

| File | Purpose | Audience | Length |
|------|---------|----------|--------|
| **[QUICKSTART.md](QUICKSTART.md)** | Fast reference guide | All users | 350 lines |
| **[WSL_BUILD_GUIDE.md](WSL_BUILD_GUIDE.md)** | Comprehensive manual | All users | 400 lines |
| **[WSL_SETUP_COMPLETE.md](WSL_SETUP_COMPLETE.md)** | Setup summary | New users | 300 lines |
| **[WSL_IMPLEMENTATION_SUMMARY.md](WSL_IMPLEMENTATION_SUMMARY.md)** | Technical details | Developers | 400 lines |
| **[DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md)** | This file | All users | You're here! |

---

## ??? Script Files

### Main Build Scripts

| Script | Platform | Purpose | Runtime |
|--------|----------|---------|---------|
| **wsl_complete_build_and_test.sh** | WSL/Linux | Full build & test | 5-10 min |
| **wsl_quick_test.sh** | WSL/Linux | Fast verification | 30-60 sec |
| **wsl_setup.sh** | WSL/Linux | Environment check | <10 sec |
| **wsl_run.sh** | WSL/Linux | Advanced runner | Variable |

### Windows Launchers

| Script | Platform | Purpose |
|--------|----------|---------|
| **wsl_build.ps1** | PowerShell | Main launcher with options |
| **wsl_build.bat** | Batch | Simple launcher |

---

## ?? Learning Path

### Beginner Level (Day 1)
1. Read **[QUICKSTART.md](QUICKSTART.md)** (10 min)
2. Run `.\wsl_build.ps1 -Setup` (1 min)
3. Run `.\wsl_build.ps1 -Quick` (2 min)
4. Success! You've verified the setup ?

### Intermediate Level (Day 2-3)
1. Read **[WSL_BUILD_GUIDE.md](WSL_BUILD_GUIDE.md)** (30 min)
2. Run full build: `.\wsl_build.ps1` (10 min)
3. Try manual commands from guide (30 min)
4. Write simple test program (1 hour)
5. Success! You can build and test ?

### Advanced Level (Week 1)
1. Read **[WSL_IMPLEMENTATION_SUMMARY.md](WSL_IMPLEMENTATION_SUMMARY.md)** (20 min)
2. Understand script internals (1 hour)
3. Modify test programs (2 hours)
4. Add custom peripherals (variable)
5. Debug with GDB (1 hour)
6. Success! You're a power user ?

---

## ?? Quick Reference

### Common Tasks

| Task | Command |
|------|---------|
| **Verify setup** | `.\wsl_build.ps1 -Setup` or `./wsl_setup.sh` |
| **Quick test** | `.\wsl_build.ps1 -Quick` or `./wsl_quick_test.sh` |
| **Full build** | `.\wsl_build.ps1` or `./wsl_complete_build_and_test.sh` |
| **Run single test** | `./build_plain/RISCV_VP -f tests/hex/test.hex -R 32` |
| **Debug with GDB** | `./build_plain/RISCV_VP -f test.hex -R 32 -D` |
| **Compare pipelines** | `./wsl_run.sh compare test.hex 200000` |

### File Locations

| Type | Location |
|------|----------|
| **Source code** | `src/*.cpp`, `inc/*.h` |
| **Test programs** | `tests/full_system/*.c` |
| **Hex files** | `tests/hex/*.hex` |
| **Executables** | `build_plain/`, `build_pipe/` |
| **Logs** | `/tmp/test_*.log` |
| **Documentation** | `*.md` files |

---

## ?? Feature Matrix

### What Works Where

| Feature | Windows Native | WSL | Native Linux |
|---------|:-------------:|:---:|:------------:|
| **Build** | ?? Limited | ? Full | ? Full |
| **Test** | ?? Limited | ? Full | ? Full |
| **GDB Debug** | ? No | ? Yes | ? Yes |
| **RISC-V Toolchain** | ? Hard | ? Easy | ? Easy |
| **Automation** | ?? Partial | ? Complete | ? Complete |

Legend: ? Fully supported | ?? Partially supported | ? Not supported

---

## ?? Troubleshooting Guide

### Common Issues

| Problem | Solution | Document |
|---------|----------|----------|
| "WSL not found" | Install WSL | [QUICKSTART.md](QUICKSTART.md#-need-help) |
| "Permission denied" | `chmod +x *.sh` | [WSL_BUILD_GUIDE.md](WSL_BUILD_GUIDE.md#troubleshooting) |
| "Build fails" | Clean and rebuild | [QUICKSTART.md](QUICKSTART.md#-troubleshooting) |
| "Tests timeout" | Use quick tests | [WSL_BUILD_GUIDE.md](WSL_BUILD_GUIDE.md#troubleshooting) |
| "Toolchain not found" | Auto-installs or manual | [WSL_SETUP_COMPLETE.md](WSL_SETUP_COMPLETE.md#-common-issues-and-fixes) |

### Getting Help

1. **Check relevant documentation** (see table above)
2. **Run setup verification:** `./wsl_setup.sh`
3. **Check logs:** `cat /tmp/test_*.log`
4. **Review error messages** (usually self-explanatory)
5. **Clean and retry:** `rm -rf build_*` and rebuild

---

## ?? Additional Resources

### Project Documentation
- **README.md** - Main project overview
- **CMakeLists.txt** - Build configuration
- **inc/*.h** - Header file documentation
- **src/*.cpp** - Implementation comments

### External Resources
- [RISC-V ISA Specification](https://riscv.org/technical/specifications/)
- [SystemC Documentation](https://www.accellera.org/downloads/standards/systemc)
- [TLM-2.0 Reference](https://www.accellera.org/downloads/standards/systemc)
- [WSL Documentation](https://docs.microsoft.com/en-us/windows/wsl/)

---

## ?? Documentation Style Guide

### Symbols Used
- ? Success / Working / Recommended
- ?? Warning / Partial / Caution  
- ? Error / Not working / Not recommended
- ?? Important / Focus here
- ?? Tip / Pro tip
- ?? Details / Deep dive
- ?? Quick / Fast
- ?? Documentation
- ??? Tool / Command
- ?? Learning / Tutorial

### Document Structure
All documentation follows this pattern:
1. **Quick reference** at top
2. **Detailed explanations** in middle
3. **Troubleshooting** near end
4. **Resources** at bottom

---

## ?? Version History

### v1.0 (Current) - WSL Complete Implementation
- ? Complete WSL build system
- ? Automated test framework
- ? Fixed Debug.cpp for Linux
- ? Three test levels
- ? Comprehensive documentation
- ? Windows launchers
- ? Performance metrics

### Previous
- Windows-only builds (limited functionality)
- Manual compilation required
- No automated testing

---

## ?? Next Steps

### Immediate (Today)
1. Run `.\wsl_build.ps1 -Setup`
2. If successful, run `.\wsl_build.ps1 -Quick`
3. If successful, run `.\wsl_build.ps1`
4. Read **[QUICKSTART.md](QUICKSTART.md)**

### Short Term (This Week)
1. Read **[WSL_BUILD_GUIDE.md](WSL_BUILD_GUIDE.md)**
2. Try manual commands
3. Write simple test program
4. Experiment with GDB debugging

### Long Term (This Month)
1. Read **[WSL_IMPLEMENTATION_SUMMARY.md](WSL_IMPLEMENTATION_SUMMARY.md)**
2. Understand build system
3. Modify existing tests
4. Add custom features

---

## ?? Pro Tips

1. **Start with setup verification** - Catches issues early
2. **Use quick tests during development** - Faster iteration
3. **Read error messages** - They're usually helpful
4. **Check logs in /tmp/** - Detailed output saved
5. **Use WSL for everything** - Better than Windows native
6. **Bookmark QUICKSTART.md** - Fast reference
7. **Keep documentation open** - While working
8. **Clean builds when in doubt** - `rm -rf build_*`

---

## ?? Support

### Where to Find Answers

| Question Type | Look Here |
|--------------|-----------|
| "How do I...?" | [QUICKSTART.md](QUICKSTART.md) or [WSL_BUILD_GUIDE.md](WSL_BUILD_GUIDE.md) |
| "What does...?" | [WSL_IMPLEMENTATION_SUMMARY.md](WSL_IMPLEMENTATION_SUMMARY.md) |
| "It's not working!" | [Troubleshooting sections](#-troubleshooting-guide) |
| "How was this built?" | [WSL_SETUP_COMPLETE.md](WSL_SETUP_COMPLETE.md) |

### Self-Help Checklist
- [ ] Read relevant documentation
- [ ] Run `./wsl_setup.sh`
- [ ] Check error message
- [ ] Try clean rebuild
- [ ] Check logs in `/tmp/`
- [ ] Review command syntax

---

## ? Summary

You have everything you need:

?? **5 comprehensive documentation files**  
??? **6 automated scripts**  
? **3 test levels**  
?? **2 Windows launchers**  
?? **1 complete build system**  

**Everything works. Everything is documented. Start now! ??**

---

## ?? Final Checklist

Before you start, verify you have:

- [x] Windows 10/11 with WSL installed
- [x] This project directory
- [x] Read at least QUICKSTART.md
- [x] Terminal or PowerShell open
- [x] 10 minutes of time
- [x] Excitement to learn! ??

**Ready? Run:** `.\wsl_build.ps1 -Setup`

**Let's go!** ??????
