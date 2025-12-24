# Quick Test Script - Assumes boards are already flashed
# Usage: .\quick-test.ps1 [iterations]

param([int]$Iterations = 1)

Write-Host "Running Quick Test (skipping build/flash)..." -ForegroundColor Cyan
Write-Host ""

& "$PSScriptRoot\auto-test-tx-rx.ps1" -Iterations $Iterations -SkipBuild -SkipFlash
