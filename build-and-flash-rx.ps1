# Build and flash Node B (RX) in one command
# Usage: .\build-and-flash-rx.ps1

Write-Host "=== Building and Flashing Node B (RX) ===" -ForegroundColor Cyan

# Configure for RX
Write-Host "`n[1/3] Configuring for RX..." -ForegroundColor Yellow
& "$PSScriptRoot\set-node-rx.ps1"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to configure for RX" -ForegroundColor Red
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
Write-Host "Select the RX board (Node B) from the list below:" -ForegroundColor Cyan
& "$PSScriptRoot\flash.ps1" -NodeType RX

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== Node B (RX) ready! ===" -ForegroundColor Green
} else {
    Write-Host "`nFlash failed!" -ForegroundColor Red
    exit 1
}

