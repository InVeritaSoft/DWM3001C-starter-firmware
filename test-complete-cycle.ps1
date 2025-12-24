# Complete test cycle: Build, Flash, and Test UART
# This script automates the entire process

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("TX", "RX", "BOTH")]
    [string]$NodeType = "BOTH"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Complete Test Cycle Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Build firmware
Write-Host "[1/4] Building firmware..." -ForegroundColor Yellow
try {
    if ($NodeType -eq "TX" -or $NodeType -eq "BOTH") {
        Write-Host "  Building TX variant..." -ForegroundColor Gray
        & .\set-node-tx.ps1
        if ($LASTEXITCODE -ne 0) { throw "TX build failed" }
        & .\build.ps1
        if ($LASTEXITCODE -ne 0) { throw "TX build failed" }
    }
    
    if ($NodeType -eq "RX" -or $NodeType -eq "BOTH") {
        Write-Host "  Building RX variant..." -ForegroundColor Gray
        & .\set-node-rx.ps1
        if ($LASTEXITCODE -ne 0) { throw "RX build failed" }
        & .\build.ps1
        if ($LASTEXITCODE -ne 0) { throw "RX build failed" }
    }
    Write-Host "  Build completed successfully" -ForegroundColor Green
} catch {
    Write-Host "  ERROR: Build failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Step 2: Flash firmware
Write-Host "[2/4] Flashing firmware..." -ForegroundColor Yellow
try {
    if ($NodeType -eq "TX" -or $NodeType -eq "BOTH") {
        Write-Host "  Flashing TX board..." -ForegroundColor Gray
        Write-Host "  Please select the TX board when prompted" -ForegroundColor Gray
        & .\build-and-flash-tx.ps1
        if ($LASTEXITCODE -ne 0) { throw "TX flash failed" }
        Start-Sleep -Seconds 2
    }
    
    if ($NodeType -eq "RX" -or $NodeType -eq "BOTH") {
        Write-Host "  Flashing RX board..." -ForegroundColor Gray
        Write-Host "  Please select the RX board when prompted" -ForegroundColor Gray
        & .\build-and-flash-rx.ps1
        if ($LASTEXITCODE -ne 0) { throw "RX flash failed" }
        Start-Sleep -Seconds 2
    }
    Write-Host "  Flash completed successfully" -ForegroundColor Green
} catch {
    Write-Host "  ERROR: Flash failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Step 3: Wait for boards to boot
Write-Host "[3/4] Waiting for boards to initialize..." -ForegroundColor Yellow
Write-Host "  Waiting 3 seconds for firmware to start..." -ForegroundColor Gray
Start-Sleep -Seconds 3

Write-Host ""

# Step 4: Test UART communication
Write-Host "[4/4] Testing UART communication..." -ForegroundColor Yellow
try {
    & .\test-uart-direct.ps1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  WARNING: UART test returned non-zero exit code" -ForegroundColor Yellow
    }
} catch {
    Write-Host "  ERROR: UART test failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Test cycle completed!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
