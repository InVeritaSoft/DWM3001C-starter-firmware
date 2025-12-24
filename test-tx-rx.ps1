# Test TX/RX Communication Script (Legacy - use auto-test-tx-rx.ps1 instead)
# This script builds, flashes both nodes, and runs the Orchestrator test

Write-Host "=== UWB TX/RX Test Script ===" -ForegroundColor Cyan
Write-Host "NOTE: Consider using auto-test-tx-rx.ps1 for better automation" -ForegroundColor Yellow
Write-Host ""

# Step 1: Build and Flash TX Node
Write-Host "[1/4] Building and Flashing TX Node (Node A)..." -ForegroundColor Yellow
& "$PSScriptRoot\build-and-flash-tx.ps1"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to flash TX node!" -ForegroundColor Red
    exit 1
}
Write-Host "TX Node flashed successfully" -ForegroundColor Green
Write-Host ""

# Step 2: Build and Flash RX Node  
Write-Host "[2/4] Building and Flashing RX Node (Node B)..." -ForegroundColor Yellow
& "$PSScriptRoot\build-and-flash-rx.ps1"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to flash RX node!" -ForegroundColor Red
    exit 1
}
Write-Host "RX Node flashed successfully" -ForegroundColor Green
Write-Host ""

# Step 3: Wait for boards to initialize
Write-Host "[3/4] Waiting for boards to initialize (5 seconds)..." -ForegroundColor Yellow
Start-Sleep -Seconds 5

# Step 4: Run Orchestrator test
Write-Host "[4/4] Starting Orchestrator test..." -ForegroundColor Yellow
Write-Host ""
Write-Host "Make sure:" -ForegroundColor Cyan
Write-Host "  - Both boards are powered on" -ForegroundColor White
Write-Host "  - RS-485 connections are correct" -ForegroundColor White
Write-Host "  - Serial ports are configured in Orchestrator/config/settings.yaml" -ForegroundColor White
Write-Host ""
Write-Host "Starting Orchestrator..." -ForegroundColor Green
Write-Host ""

Set-Location Orchestrator
npm start
