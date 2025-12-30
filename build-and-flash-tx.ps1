# Build and flash Node A (TX v2) in one command
# Usage: .\build-and-flash-tx.ps1

Write-Host "=== Building and Flashing Node A (TX v2) ===" -ForegroundColor Cyan

# Configure for TX v2
Write-Host "`n[1/3] Configuring for TX v2..." -ForegroundColor Yellow
& "$PSScriptRoot\set-node-tx-v2.ps1"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to configure for TX" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "`n[2/3] Building firmware..." -ForegroundColor Yellow
& "$PSScriptRoot\build.ps1"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# Flash
Write-Host "`n[3/3] Flashing to board..." -ForegroundColor Yellow
Write-Host "Select the TX board (Node A) from the list below:" -ForegroundColor Cyan
& "$PSScriptRoot\flash.ps1" -NodeType TX

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== Node A (TX) ready! ===" -ForegroundColor Green
} else {
    Write-Host "`nFlash failed!" -ForegroundColor Red
    exit 1
}

