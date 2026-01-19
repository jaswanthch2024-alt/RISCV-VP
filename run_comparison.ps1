# PowerShell script to run performance comparison in WSL
# Compares TLM vs VP and Pipelined vs Non-Pipelined

param(
    [string]$HexFile = "tests/hex/robust_fast64.hex",
    [string]$Arch = "64"
)

$ErrorActionPreference = "Stop"

Write-Host "`n====================================================" -ForegroundColor Cyan
Write-Host "  RISC-V-TLM Performance Comparison Runner" -ForegroundColor Cyan
Write-Host "  TLM vs VP | Pipelined vs Non-Pipelined" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan

# Check WSL
Write-Host "`nChecking WSL..." -ForegroundColor Yellow
try {
    $wslCheck = wsl --status 2>&1
    Write-Host "? WSL is available" -ForegroundColor Green
} catch {
    Write-Host "? WSL not found!" -ForegroundColor Red
    Write-Host "`nInstall WSL with: wsl --install" -ForegroundColor Yellow
    exit 1
}

# Get current directory and convert to WSL path
$currentDir = Get-Location
$wslPath = wsl wslpath -a $currentDir.Path

Write-Host "`nProject directory: $currentDir" -ForegroundColor Cyan
Write-Host "WSL path: $wslPath" -ForegroundColor Cyan
Write-Host ""

# Display configuration
Write-Host "Test Configuration:" -ForegroundColor Yellow
Write-Host "  Hex File: $HexFile" -ForegroundColor White
Write-Host "  Architecture: RV$Arch" -ForegroundColor White
Write-Host ""

Write-Host "Starting comparison tests..." -ForegroundColor Yellow
Write-Host "This will take approximately 2-5 minutes depending on your system.`n" -ForegroundColor Gray

# Run the comparison script in WSL
try {
    wsl bash -c "cd '$wslPath' && chmod +x wsl_run_comparison.sh && ./wsl_run_comparison.sh $HexFile $Arch"
    
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        Write-Host "`n====================================================" -ForegroundColor Green
        Write-Host "  ? Comparison Complete!" -ForegroundColor Green
        Write-Host "====================================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "A detailed report has been saved in the project directory." -ForegroundColor Cyan
        Write-Host "Look for: comparison_report_*.txt" -ForegroundColor White
        Write-Host ""
    } else {
        Write-Host "`n? Comparison failed (exit code: $exitCode)" -ForegroundColor Red
        exit $exitCode
    }
    
} catch {
    Write-Host "`n? Error running comparison!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

# Offer to view the report
Write-Host "Options:" -ForegroundColor Cyan
Write-Host "  1. View the report: Get-Content comparison_report_*.txt | Select-Object -Last 50" -ForegroundColor White
Write-Host "  2. Run different test: .\run_comparison.ps1 -HexFile 'path/to/test.hex' -Arch 32" -ForegroundColor White
Write-Host ""

exit 0
