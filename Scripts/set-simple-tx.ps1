# Configure firmware for Simple TX example
# Usage: .\set-simple-tx.ps1

$ErrorActionPreference = "Stop"

try {
    Write-Host "Configuring firmware for Simple TX example..." -ForegroundColor Green

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

    # Comment out all other examples
    $content = $content -replace '(?m)^#define TEST_READING_DEV_ID', '//#define TEST_READING_DEV_ID'
    $content = $content -replace '(?m)^#define TEST_SIMPLE_RX', '//#define TEST_SIMPLE_RX'
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_TX', '//#define TEST_ORCHESTRATOR_TX'
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_RX', '//#define TEST_ORCHESTRATOR_RX'
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_TX_V2', '//#define TEST_ORCHESTRATOR_TX_V2'
    $content = $content -replace '(?m)^#define TEST_ORCHESTRATOR_RX_V2', '//#define TEST_ORCHESTRATOR_RX_V2'

    # Uncomment TEST_SIMPLE_TX
    $content = $content -replace '(?m)^//#define TEST_SIMPLE_TX', '#define TEST_SIMPLE_TX'

    # Write back
    Set-Content -Path $exampleSelectionFile -Value $content -NoNewline

    # Read main.c
    $mainContent = Get-Content $mainFile -Raw

    # Comment out all other examples
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int read_dev_id\(void\); read_dev_id\(\);', '$1// extern int read_dev_id(void); read_dev_id();'
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int simple_rx\(void\); simple_rx\(\);', '$1// extern int simple_rx(void); simple_rx();'
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int orchestrator_tx\(void\); orchestrator_tx\(\);', '$1// extern int orchestrator_tx(void); orchestrator_tx();'
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int orchestrator_rx\(void\); orchestrator_rx\(\);', '$1// extern int orchestrator_rx(void); orchestrator_rx();'
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int orchestrator_tx_v2\(void\); orchestrator_tx_v2\(\);', '$1// extern int orchestrator_tx_v2(void); orchestrator_tx_v2();'
    $mainContent = $mainContent -replace '(?m)^(\s*)(//\s*)?extern int orchestrator_rx_v2\(void\); orchestrator_rx_v2\(\);', '$1// extern int orchestrator_rx_v2(void); orchestrator_rx_v2();'

    # Uncomment simple_tx
    $mainContent = $mainContent -replace '(?m)^(\s*)//\s*extern int simple_tx\(void\); simple_tx\(\);', '$1extern int simple_tx(void); simple_tx();'

    # Write back
    Set-Content -Path $mainFile -Value $mainContent -NoNewline

    Write-Host "Configuration complete!" -ForegroundColor Green
    Write-Host "  - Simple TX example enabled" -ForegroundColor Cyan
    Write-Host "  - Other examples disabled" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "  1. Build: make build" -ForegroundColor White
    Write-Host "  2. Flash: make flash" -ForegroundColor White
    Write-Host ""
    Write-Host "The Simple TX example will:" -ForegroundColor Yellow
    Write-Host "  - Transmit UWB frames every 500ms" -ForegroundColor White
    Write-Host "  - Use channel 5, 6.8Mbps, preamble length 128" -ForegroundColor White
    Write-Host "  - Send blink frames with sequence numbers" -ForegroundColor White

} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}

