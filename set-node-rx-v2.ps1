# Configure firmware for Node B (RX v2 variant)
# Usage: .\set-node-rx-v2.ps1

$ErrorActionPreference = "Stop"

try {
    Write-Host "Configuring firmware for Node B (RX v2)..." -ForegroundColor Green

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

    # Comment out all orchestrator variants
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_TX', '//#define TEST_ORCHESTRATOR_TX'
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_RX', '//#define TEST_ORCHESTRATOR_RX'
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_TX_V2', '//#define TEST_ORCHESTRATOR_TX_V2'

    # Uncomment TEST_ORCHESTRATOR_RX_V2
    $content = $content -replace '(?m)^//#define TEST_ORCHESTRATOR_RX_V2', '#define TEST_ORCHESTRATOR_RX_V2'

    # Write back
    Set-Content -Path $exampleSelectionFile -Value $content -NoNewline

    # Read main.c
    $mainContent = Get-Content $mainFile -Raw

    # Comment out read_dev_id
    $mainContent = $mainContent -replace '(?m)^(\s*)extern int read_dev_id\(void\); read_dev_id\(\);', '$1// extern int read_dev_id(void); read_dev_id();'

    # Comment out all orchestrator variants (handle both commented and uncommented)
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int orchestrator_tx\(void\); orchestrator_tx\(\);', '$1// extern int orchestrator_tx(void); orchestrator_tx();'
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int orchestrator_rx\(void\); orchestrator_rx\(\);', '$1// extern int orchestrator_rx(void); orchestrator_rx();'
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int orchestrator_tx_v2\(void\); orchestrator_tx_v2\(\);', '$1// extern int orchestrator_tx_v2(void); orchestrator_tx_v2();'

    # Uncomment orchestrator_rx_v2 (remove // if present)
    $mainContent = $mainContent -replace '(?m)^(\s*)//\s*extern int orchestrator_rx_v2\(void\); orchestrator_rx_v2\(\);', '$1extern int orchestrator_rx_v2(void); orchestrator_rx_v2();'
    # If not found at all, add it after the orchestrator_rx line
    if ($mainContent -notmatch 'extern int orchestrator_rx_v2\(void\); orchestrator_rx_v2\(\);') {
        $mainContent = $mainContent -replace '(?m)(\s*)(// extern int orchestrator_rx\(void\); orchestrator_rx\(\);)', '$1extern int orchestrator_rx_v2(void); orchestrator_rx_v2();$2'
    }

    # Write back
    Set-Content -Path $mainFile -Value $mainContent -NoNewline

    Write-Host "Configuration updated for Node B (RX v2)" -ForegroundColor Green
    Write-Host "Run .\build.ps1 to build the firmware" -ForegroundColor Cyan
    
    exit 0
} catch {
    Write-Host "Error configuring for RX v2: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

