# Diagnostic script to find why UWB transmission isn't starting
# Even though RS-485 commands are being received (LED blinks)
# Usage: .\diagnose-uwb-issue.ps1 <COM_PORT>
# Example: .\diagnose-uwb-issue.ps1 COM3

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=== UWB Transmission Diagnostic ===" -ForegroundColor Cyan
Write-Host "Diagnosing why UWB packets aren't being exchanged..." -ForegroundColor Yellow
Write-Host ""

# Open serial port
try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 3000
    $port.WriteTimeout = 3000
    $port.Open()
    Write-Host "✓ Serial port opened: $ComPort" -ForegroundColor Green
} catch {
    Write-Host "✗ Failed to open serial port: $_" -ForegroundColor Red
    exit 1
}

function Send-Command {
    param([string]$cmd, [int]$timeoutMs = 2000)
    Write-Host "→ $cmd" -ForegroundColor Cyan
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    $port.WriteLine($cmd)
    Start-Sleep -Milliseconds 100
    
    $response = ""
    $timeout = [math]::Ceiling($timeoutMs / 20)
    while ($timeout -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $response += $port.ReadExisting()
            if ($response -match "OK|ERR|PNG") {
                break
            }
        }
        Start-Sleep -Milliseconds 20
        $timeout--
    }
    
    if ($response) {
        $response = $response.Trim()
        Write-Host "← $response" -ForegroundColor $(if ($response -match "^OK") { "Green" } elseif ($response -match "^ERR") { "Red" } else { "Yellow" })
    } else {
        Write-Host "← (no response)" -ForegroundColor Yellow
    }
    Write-Host ""
    return $response
}

Write-Host "--- Step 1: Verify RS-485 Communication ---" -ForegroundColor Magenta
$ping = Send-Command "PNG"
if (-not ($ping -match "OK")) {
    Write-Host "✗ CRITICAL: RS-485 communication failed!" -ForegroundColor Red
    Write-Host "  Cannot proceed. Check wiring and baud rate." -ForegroundColor Red
    $port.Close()
    exit 1
}
Write-Host "✓ RS-485 communication working (LED should blink)" -ForegroundColor Green
Write-Host ""

Write-Host "--- Step 2: Check Node Type ---" -ForegroundColor Magenta
$nodeType = Send-Command "NODE_TYPE"
$isTX = $nodeType -match "TX"
$isRX = $nodeType -match "RX"
Write-Host "Node Type: $(if ($isTX) { 'TX (Transmitter)' } elseif ($isRX) { 'RX (Receiver)' } else { 'Unknown' })" -ForegroundColor $(if ($isTX -or $isRX) { "Green" } else { "Yellow" })
Write-Host ""

Write-Host "--- Step 3: Check Current Configuration Status ---" -ForegroundColor Magenta
Write-Host "Checking if UWB is configured..." -ForegroundColor White

# Try to start - this will tell us if configured
$startTest = Send-Command "STRT"
if ($startTest -match "ERR NOT_CONFIGURED") {
    Write-Host "✗ ISSUE FOUND: UWB not configured!" -ForegroundColor Red
    Write-Host "  Solution: Send CFG command first" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Configuring UWB now..." -ForegroundColor Cyan
    $config = Send-Command "CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100"
    if (-not ($config -match "OK CONFIG")) {
        Write-Host "✗ Configuration failed!" -ForegroundColor Red
        $port.Close()
        exit 1
    }
    Write-Host "✓ UWB configured successfully" -ForegroundColor Green
} elseif ($startTest -match "ERR TIMER_NOT_INIT") {
    Write-Host "✗ ISSUE FOUND: Timer not initialized!" -ForegroundColor Red
    Write-Host "  This is a firmware issue - timer initialization failed during boot." -ForegroundColor Yellow
    Write-Host "  Check RTT logs for 'TIMER INIT FAILED' message." -ForegroundColor Yellow
    Write-Host "  Possible causes:" -ForegroundColor Yellow
    Write-Host "    - Timer module initialization error" -ForegroundColor White
    Write-Host "    - Insufficient memory" -ForegroundColor White
    Write-Host "    - Hardware issue" -ForegroundColor White
    $port.Close()
    exit 1
} elseif ($startTest -match "ERR START_FAILED") {
    Write-Host "✗ ISSUE FOUND: Timer start failed!" -ForegroundColor Red
    Write-Host "  Timer exists but cannot be started." -ForegroundColor Yellow
    $port.Close()
    exit 1
} elseif ($startTest -match "OK START") {
    Write-Host "✓ START command succeeded" -ForegroundColor Green
} else {
    Write-Host "⚠ Unexpected response to START command" -ForegroundColor Yellow
}

Write-Host ""

Write-Host "--- Step 4: Check Statistics After 3 Seconds ---" -ForegroundColor Magenta
Write-Host "Waiting 3 seconds for packets to be sent/received..." -ForegroundColor White
Start-Sleep -Seconds 3

$stats = Send-Command "STAT"

# Parse statistics
if ($isTX) {
    if ($stats -match "sent=(\d+)") {
        $sent = [int]$matches[1]
        if ($sent -eq 0) {
            Write-Host "✗ PROBLEM: No packets sent (sent=0)" -ForegroundColor Red
            Write-Host ""
            Write-Host "Possible causes:" -ForegroundColor Yellow
            Write-Host "  1. Timer not firing (check RTT logs)" -ForegroundColor White
            Write-Host "  2. g_test_running flag not set" -ForegroundColor White
            Write-Host "  3. send_packet() function failing silently" -ForegroundColor White
            Write-Host "  4. DW3000 hardware issue" -ForegroundColor White
        } else {
            Write-Host "✓ Packets are being sent! (sent=$sent)" -ForegroundColor Green
        }
    }
    
    if ($stats -match "attempted=(\d+)") {
        $attempted = [int]$matches[1]
        if ($attempted -gt 0 -and $sent -eq 0) {
            Write-Host "⚠ Attempts made but no successful sends" -ForegroundColor Yellow
            Write-Host "  This suggests transmission errors" -ForegroundColor Yellow
        }
    }
    
    if ($stats -match "errors=(\d+)") {
        $errors = [int]$matches[1]
        if ($errors -gt 0) {
            Write-Host "⚠ Transmission errors detected: $errors" -ForegroundColor Yellow
        }
    }
    
    if ($stats -match "timeouts=(\d+)") {
        $timeouts = [int]$matches[1]
        if ($timeouts -gt 0) {
            Write-Host "⚠ Transmission timeouts detected: $timeouts" -ForegroundColor Yellow
            Write-Host "  DW3000 may not be responding to TX commands" -ForegroundColor Yellow
        }
    }
} elseif ($isRX) {
    if ($stats -match "rx=(\d+)") {
        $rx = [int]$matches[1]
        if ($rx -eq 0) {
            Write-Host "✗ PROBLEM: No packets received (rx=0)" -ForegroundColor Red
            Write-Host ""
            Write-Host "Possible causes:" -ForegroundColor Yellow
            Write-Host "  1. TX node not transmitting (check TX node)" -ForegroundColor White
            Write-Host "  2. Configuration mismatch (channel, rate, preamble)" -ForegroundColor White
            Write-Host "  3. RX not enabled (dwt_rxenable not called)" -ForegroundColor White
            Write-Host "  4. Physical distance too far" -ForegroundColor White
            Write-Host "  5. Antenna issues" -ForegroundColor White
        } else {
            Write-Host "✓ Packets are being received! (rx=$rx)" -ForegroundColor Green
        }
    }
    
    if ($stats -match "crc_err=(\d+)") {
        $crcErr = [int]$matches[1]
        if ($crcErr -gt 0) {
            Write-Host "⚠ CRC errors detected: $crcErr" -ForegroundColor Yellow
            Write-Host "  Packets received but corrupted" -ForegroundColor Yellow
        }
    }
}

Write-Host ""
Write-Host "--- Step 5: Verify Both Nodes Are Started ---" -ForegroundColor Magenta
Write-Host "IMPORTANT: Both TX and RX nodes need START command!" -ForegroundColor Yellow
Write-Host "  - TX node: Starts timer to send packets" -ForegroundColor White
Write-Host "  - RX node: Enables RX to listen for packets" -ForegroundColor White
Write-Host ""
Write-Host "Current node status:" -ForegroundColor Cyan
if ($startTest -match "OK START") {
    Write-Host "  ✓ This node is STARTED" -ForegroundColor Green
} else {
    Write-Host "  ✗ This node is NOT started" -ForegroundColor Red
    Write-Host "    Run this script on the other node too!" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Diagnostic Summary ===" -ForegroundColor Cyan
Write-Host "RS-485 Communication: ✓ Working (LED blinks)" -ForegroundColor Green
Write-Host "Node Type: $(if ($isTX) { 'TX' } elseif ($isRX) { 'RX' } else { 'Unknown' })" -ForegroundColor $(if ($isTX -or $isRX) { "Green" } else { "Yellow" })
Write-Host "UWB Configured: $(if ($startTest -match 'OK START' -or $config -match 'OK CONFIG') { 'Yes' } else { 'No' })" -ForegroundColor $(if ($startTest -match 'OK START' -or $config -match 'OK CONFIG') { "Green" } else { "Red" })
Write-Host "Test Started: $(if ($startTest -match 'OK START') { 'Yes' } else { 'No' })" -ForegroundColor $(if ($startTest -match 'OK START') { "Green" } else { "Red" })

if ($isTX) {
    Write-Host "Packets Sent: $(if ($stats -match 'sent=(\d+)') { $matches[1] } else { 'Unknown' })" -ForegroundColor $(if ($stats -match 'sent=(\d+)' -and [int]$matches[1] -gt 0) { "Green" } else { "Yellow" })
} elseif ($isRX) {
    Write-Host "Packets Received: $(if ($stats -match 'rx=(\d+)') { $matches[1] } else { 'Unknown' })" -ForegroundColor $(if ($stats -match 'rx=(\d+)' -and [int]$matches[1] -gt 0) { "Green" } else { "Yellow" })
}

Write-Host ""
Write-Host "=== Next Steps ===" -ForegroundColor Cyan
if ($isTX -and $stats -match "sent=0") {
    Write-Host "1. Check RTT logs for timer-related errors" -ForegroundColor Yellow
    Write-Host "2. Verify timer initialization succeeded during boot" -ForegroundColor Yellow
    Write-Host "3. Check if tx_timer_handler() is being called" -ForegroundColor Yellow
    Write-Host "4. Verify DW3000 initialization succeeded" -ForegroundColor Yellow
} elseif ($isRX -and $stats -match "rx=0") {
    Write-Host "1. Run this script on TX node to verify it's transmitting" -ForegroundColor Yellow
    Write-Host "2. Verify both nodes have identical UWB configuration" -ForegroundColor Yellow
    Write-Host "3. Check physical distance between nodes" -ForegroundColor Yellow
    Write-Host "4. Verify antennas are connected properly" -ForegroundColor Yellow
} else {
    Write-Host "✓ Everything appears to be working!" -ForegroundColor Green
    Write-Host "  If packets still aren't exchanging, check:" -ForegroundColor Yellow
    Write-Host "  - Configuration mismatch between nodes" -ForegroundColor White
    Write-Host "  - Physical distance/obstacles" -ForegroundColor White
    Write-Host "  - Antenna connections" -ForegroundColor White
}

$port.Close()

