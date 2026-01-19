@echo off
REM SPDX-License-Identifier: GPL-3.0-or-later
REM Windows launcher for WSL migration script
REM Usage: Double-click or run from PowerShell/CMD

echo ==========================================
echo RISC-V TLM WSL Migration Launcher
echo ==========================================
echo.
echo This will:
echo   1. Copy project to WSL native filesystem
echo   2. Install dependencies (cmake, ninja, etc.)
echo   3. Install SystemC if needed
echo   4. Build both RISCV_TLM and RISCV_VP
echo.
echo Press Ctrl+C to cancel, or
pause

wsl bash -lc "cd /mnt/c/Users/jaswa/Source/Repos/RISC-V-TLM && bash migrate_to_wsl.sh"

if %ERRORLEVEL% EQU 0 (
  echo.
  echo ==========================================
  echo SUCCESS! Project migrated and built.
  echo ==========================================
  echo.
  echo To run tests, open WSL and type:
  echo   cd ~/RISC-V-TLM
  echo   ./wsl_run_all_tests.sh
  echo.
) else (
  echo.
  echo ==========================================
  echo Migration failed. Check output above.
  echo ==========================================
)

pause
