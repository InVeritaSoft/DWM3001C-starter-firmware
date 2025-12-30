# Test UWB Transmission on Orchestrator TX Node
# This script sends commands to verify UWB transmission is working
# Usage: .\test-uwb-transmission.ps1 <COM_PORT>
# Example: .\test-uwb-transmission.ps1 COM3

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=== UWB Transmission Diagnostic Test ===" -ForegroundColor Cyan
Write-Host "Testing COM Port: $ComPort" -ForegroundColor Yellow
Write-Host ""

# Open serial port
try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    $port.Open()
    Write-Host "✓ Serial port opened successfully" -ForegroundColor Green
} catch {
    Write-Host "✗ Failed to open serial port: $_" -ForegroundColor Red
    exit 1
}

function Send-Command {
    param([string]$cmd)
    Write-Host "→ Sending: $cmd" -ForegroundColor Cyan
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    $port.WriteLine($cmd)
    Start-Sleep -Milliseconds 300  # Increased delay for response
    
    $response = ""
    $timeout = 100  # 100 * 20ms = 2 second timeout
    $lastRead = ""
    while ($timeout -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $newData = $port.ReadExisting()
            $response += $newData
            $lastRead = $newData
            # Look for response patterns (OK, ERR, or end of line)
            if ($response -match "(OK|ERR)" -or ($lastRead -match "`r`n" -and $response.Length -gt 0)) {
                Start-Sleep -Milliseconds 50  # Small delay to get full response
                if ($port.BytesToRead -gt 0) {
                    $response += $port.ReadExisting()
                }
                break
            }
        } else {
            # If we got some data but no more is coming, break
            if ($response.Length -gt 0 -and $lastRead -match "`r`n") {
                break
            }
        }
        Start-Sleep -Milliseconds 20
        $timeout--
    }
    
    # Clean up response (remove echo, whitespace, etc.)
    $response = $response.Trim()
    # Remove command echo if present
    $response = $response -replace "^$cmd\s*", ""
    $response = $response.Trim()
    
    if ($response) {
        Write-Host "← Response: '$response'" -ForegroundColor $(if ($response -match "^OK") { "Green" } elseif ($response -match "^ERR") { "Red" } else { "Yellow" })
    } else {
        Write-Host "← No response (timeout)" -ForegroundColor Yellow
    }
    Write-Host ""
    return $response
}

# Test 1: Ping
Write-Host "--- Test 1: RS-485 Communication ---" -ForegroundColor Magenta
$pingResponse = Send-Command "PNG"
if (-not ($pingResponse -match "^OK")) {
    Write-Host "⚠ RS-485 response unexpected: '$pingResponse'" -ForegroundColor Yellow
    Write-Host "  Expected: 'OK'" -ForegroundColor Yellow
    Write-Host "  Received: '$pingResponse'" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  This might be:" -ForegroundColor Yellow
    Write-Host "    - Command echo (device echoing back command)" -ForegroundColor White
    Write-Host "    - Different response format" -ForegroundColor White
    Write-Host "    - Timing issue" -ForegroundColor White
    Write-Host ""
    Write-Host "  Continuing anyway to check other commands..." -ForegroundColor Cyan
    Write-Host ""
}

# Test 2: Node Type
Write-Host "--- Test 2: Node Type Check ---" -ForegroundColor Magenta
$nodeType = Send-Command "NODE_TYPE"
if (-not ($nodeType -match "TX")) {
    Write-Host "⚠ Warning: This doesn't appear to be a TX node!" -ForegroundColor Yellow
    Write-Host "  Expected: NODE_TYPE=TX or NODE_TYPE=TX_V2" -ForegroundColor Yellow
}

# Test 3: Configuration
Write-Host "--- Test 3: UWB Configuration ---" -ForegroundColor Magenta
Write-Host "Configuring UWB with default parameters..." -ForegroundColor White
$configResponse = Send-Command "CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100"
if (-not ($configResponse -match "OK CONFIG")) {
    Write-Host "✗ Configuration failed!" -ForegroundColor Red
    Write-Host "  UWB transmission cannot start without configuration." -ForegroundColor Red
    $port.Close()
    exit 1
}

# Test 4: Start Transmission
Write-Host "--- Test 4: Start Transmission ---" -ForegroundColor Magenta
$startResponse = Send-Command "STRT"
if (-not ($startResponse -match "OK START")) {
    Write-Host "✗ Failed to start transmission!" -ForegroundColor Red
    
    if ($startResponse -match "NOT_CONFIGURED") {
        Write-Host "  Reason: UWB not configured. Run CFG command first." -ForegroundColor Yellow
    } elseif ($startResponse -match "TIMER_NOT_INIT") {
        Write-Host "  Reason: Timer not initialized. Check firmware logs." -ForegroundColor Yellow
    } else {
        Write-Host "  Reason: Unknown error. Check response above." -ForegroundColor Yellow
    }
    $port.Close()
    exit 1
}

# Test 5: Check Statistics (wait a bit for packets to be sent)
Write-Host "--- Test 5: Statistics Check (waiting 2 seconds) ---" -ForegroundColor Magenta
Start-Sleep -Seconds 2

$statsResponse = Send-Command "STAT"
if ($statsResponse -match "sent=(\d+)") {
    $sentCount = $matches[1]
    Write-Host "✓ Packets sent: $sentCount" -ForegroundColor Green
    
    if ([int]$sentCount -eq 0) {
        Write-Host "⚠ Warning: No packets sent yet!" -ForegroundColor Yellow
        Write-Host "  This could indicate:" -ForegroundColor Yellow
        Write-Host "    - Timer not firing" -ForegroundColor White
        Write-Host "    - DW3000 hardware issue" -ForegroundColor White
        Write-Host "    - Transmission errors" -ForegroundColor White
    } else {
        Write-Host "✓ UWB transmission is working!" -ForegroundColor Green
    }
} else {
    Write-Host "⚠ Could not parse statistics" -ForegroundColor Yellow
}

# Test 6: Check for errors
if ($statsResponse -match "errors=(\d+)") {
    $errorCount = $matches[1]
    if ([int]$errorCount -gt 0) {
        Write-Host "⚠ Transmission errors detected: $errorCount" -ForegroundColor Yellow
    }
}

if ($statsResponse -match "timeouts=(\d+)") {
    $timeoutCount = $matches[1]
    if ([int]$timeoutCount -gt 0) {
        Write-Host "⚠ Transmission timeouts detected: $timeoutCount" -ForegroundColor Yellow
    }
}

# Summary
Write-Host ""
Write-Host "=== Test Summary ===" -ForegroundColor Cyan
Write-Host "RS-485 Communication: $(if ($pingResponse -match 'OK') { '✓ Working' } else { '✗ Failed' })" -ForegroundColor $(if ($pingResponse -match 'OK') { 'Green' } else { 'Red' })
Write-Host "UWB Configuration: $(if ($configResponse -match 'OK CONFIG') { '✓ Configured' } else { '✗ Failed' })" -ForegroundColor $(if ($configResponse -match 'OK CONFIG') { 'Green' } else { 'Red' })
Write-Host "Transmission Started: $(if ($startResponse -match 'OK START') { '✓ Started' } else { '✗ Failed' })" -ForegroundColor $(if ($startResponse -match 'OK START') { 'Green' } else { 'Red' })
Write-Host "Packets Transmitted: $(if ($statsResponse -match 'sent=(\d+)') { $matches[1] } else { 'Unknown' })" -ForegroundColor $(if ($statsResponse -match 'sent=(\d+)' -and [int]$matches[1] -gt 0) { 'Green' } else { 'Yellow' })

$port.Close()
Write-Host ""
Write-Host "Test complete!" -ForegroundColor Green

