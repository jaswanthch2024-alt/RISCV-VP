@echo off
REM Windows batch script to run WSL build and test commands
REM This makes it easy to run WSL commands from Windows

setlocal

set WSL_SCRIPT=%~dp0wsl_complete_build_and_test.sh

echo ===================================
echo RISC-V-TLM WSL Build and Test
echo ===================================
echo.

if "%1"=="help" goto :help
if "%1"=="/?" goto :help
if "%1"=="-h" goto :help

REM Convert Windows path to WSL path
for /f "tokens=*" %%i in ('wsl wslpath -a "%~dp0"') do set WSL_PATH=%%i

echo Running in WSL...
echo.

wsl bash -c "cd '%WSL_PATH%' && chmod +x wsl_complete_build_and_test.sh && ./wsl_complete_build_and_test.sh"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===================================
    echo Build and Test Completed Successfully
    echo ===================================
) else (
    echo.
    echo ===================================
    echo Build or Test Failed
    echo ===================================
    exit /b 1
)

goto :eof

:help
echo Usage: wsl_build.bat
echo.
echo This script will:
echo   1. Check and install required dependencies in WSL
echo   2. Build both pipelined and non-pipelined configurations
echo   3. Compile robust test files to hex format
echo   4. Run comprehensive tests
echo.
echo Requirements:
echo   - WSL (Windows Subsystem for Linux) must be installed
echo   - Ubuntu or similar Linux distribution in WSL
echo.
echo To install WSL:
echo   wsl --install
echo.
:eof
