# PowerShell script to setup and run WSL builds
# Run this from Windows PowerShell in the project directory

param(
    [switch]$Setup,      # Just run setup verification
    [switch]$Quick,      # Run quick tests only
    [switch]$Full,       # Run full build and test (default)
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host @"

RISC-V-TLM WSL Build and Test Runner
====================================

Usage: .\wsl_build.ps1 [options]

Options:
  -Setup    Run setup verification only
  -Quick    Build and run quick tests only
  -Full     Run complete build and test suite (default)
  -Help     Show this help message

Examples:
  .\wsl_build.ps1          # Full build and test
  .\wsl_build.ps1 -Quick   # Quick verification
  .\wsl_build.ps1 -Setup   # Verify setup only

Requirements:
  - WSL must be installed (wsl --install)
  - Ubuntu or similar Linux distribution

"@
    exit 0
}

if ($Help) {
    Show-Help
}

Write-Host "`n==================================" -ForegroundColor Cyan
Write-Host "  RISC-V-TLM WSL Build and Test" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan

# Check if WSL is available
Write-Host "`nChecking WSL..." -ForegroundColor Yellow
try {
    $wslCheck = wsl --status 2>&1
    Write-Host "? WSL is available" -ForegroundColor Green
} catch {
    Write-Host "? WSL not found!" -ForegroundColor Red
    Write-Host "`nTo install WSL, run in PowerShell as Administrator:" -ForegroundColor Yellow
    Write-Host "  wsl --install" -ForegroundColor White
    Write-Host "`nThen restart your computer." -ForegroundColor Yellow
    exit 1
}

# Get current directory and convert to WSL path
$currentDir = Get-Location
Write-Host "`nProject directory: $currentDir" -ForegroundColor Cyan

# Convert Windows path to WSL path
$wslPath = wsl wslpath -a $currentDir.Path
Write-Host "WSL path: $wslPath" -ForegroundColor Cyan

# Determine which script to run
$scriptToRun = if ($Setup) {
    "wsl_setup.sh"
} elseif ($Quick) {
    "wsl_quick_test.sh"
} else {
    "wsl_complete_build_and_test.sh"
}

$actionName = if ($Setup) {
    "Setup Verification"
} elseif ($Quick) {
    "Quick Test"
} else {
    "Full Build and Test"
}

Write-Host "`nRunning: $actionName" -ForegroundColor Yellow
Write-Host "Script: $scriptToRun" -ForegroundColor Cyan
Write-Host "`n----------------------------------`n" -ForegroundColor Gray

# Run the script in WSL
try {
    wsl bash -c "cd '$wslPath' && chmod +x *.sh && ./$scriptToRun"
    
    $exitCode = $LASTEXITCODE
    
    Write-Host "`n----------------------------------" -ForegroundColor Gray
    
    if ($exitCode -eq 0) {
        Write-Host "`n? $actionName completed successfully!" -ForegroundColor Green
        
        if ($Setup) {
            Write-Host "`nNext step: Run full build" -ForegroundColor Cyan
            Write-Host "  .\wsl_build.ps1" -ForegroundColor White
        } elseif ($Quick) {
            Write-Host "`nFor full test suite:" -ForegroundColor Cyan
            Write-Host "  .\wsl_build.ps1 -Full" -ForegroundColor White
        }
    } else {
        Write-Host "`n? $actionName failed (exit code: $exitCode)" -ForegroundColor Red
        Write-Host "`nCheck the output above for errors." -ForegroundColor Yellow
        exit $exitCode
    }
    
} catch {
    Write-Host "`n? Error running WSL script!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

Write-Host "`n==================================" -ForegroundColor Cyan
Write-Host "        Operation Complete" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""

exit 0
