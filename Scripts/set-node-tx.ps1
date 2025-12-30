# Configure firmware for Node A (TX variant)
# Usage: .\set-node-tx.ps1

$ErrorActionPreference = "Stop"

try {
    Write-Host "Configuring firmware for Node A (TX)..." -ForegroundColor Green

    $projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
    $exampleSelectionFile = Join-Path $projectRoot "Src\example_selection.h"
    $mainFile = Join-Path $projectRoot "Src\main.c"

    # Verify files exist
    if (-not (Test-Path $exampleSelectionFile)) {
        Write-Host "Error: $exampleSelectionFile not found!" -ForegroundColor Red
        exit 1
    }
    if (-not (Test-Path $mainFile)) {
        Write-Host "Error: $mainFile not found!" -ForegroundColor Red
        exit 1
    }

    # Read example_selection.h
    $content = Get-Content $exampleSelectionFile -Raw

    # Comment out TEST_READING_DEV_ID
    $content = $content -replace '(?m)^#define TEST_READING_DEV_ID', '//#define TEST_READING_DEV_ID'

    # Uncomment TEST_ORCHESTRATOR_TX
    $content = $content -replace '(?m)^//#define TEST_ORCHESTRATOR_TX', '#define TEST_ORCHESTRATOR_TX'

    # Comment out TEST_ORCHESTRATOR_RX
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_RX', '//#define TEST_ORCHESTRATOR_RX'

    # Write back
    Set-Content -Path $exampleSelectionFile -Value $content -NoNewline

    # Read main.c
    $mainContent = Get-Content $mainFile -Raw

    # Comment out read_dev_id
    $mainContent = $mainContent -replace '(?m)^(\s*)extern int read_dev_id\(void\); read_dev_id\(\);', '$1// extern int read_dev_id(void); read_dev_id();'

    # Comment out orchestrator_rx
    $mainContent = $mainContent -replace '(?m)^(\s*)// extern int orchestrator_rx\(void\); orchestrator_rx\(\);', '$1// extern int orchestrator_rx(void); orchestrator_rx();'
    $mainContent = $mainContent -replace '(?m)^(\s*)extern int orchestrator_rx\(void\); orchestrator_rx\(\);', '$1// extern int orchestrator_rx(void); orchestrator_rx();'

    # Uncomment orchestrator_tx
    $mainContent = $mainContent -replace '(?m)^(\s*)// extern int orchestrator_tx\(void\); orchestrator_tx\(\);', '$1extern int orchestrator_tx(void); orchestrator_tx();'

    # Write back
    Set-Content -Path $mainFile -Value $mainContent -NoNewline

    Write-Host "Configuration updated for Node A (TX)" -ForegroundColor Green
    Write-Host "Run .\build.ps1 to build the firmware" -ForegroundColor Cyan
    
    exit 0
} catch {
    Write-Host "Error configuring for TX: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

