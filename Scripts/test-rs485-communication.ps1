# Test RS-485 communication with proper timing for direction switching
# Usage: .\test-rs485-communication.ps1 <COM_PORT>
# Example: .\test-rs485-communication.ps1 COM11

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=== RS-485 Communication Test ===" -ForegroundColor Cyan
Write-Host "COM Port: $ComPort" -ForegroundColor Yellow
Write-Host ""
Write-Host "NOTE: Firmware uses RS-485 direction control (DE/RE pin)" -ForegroundColor Gray
Write-Host "      Responses may take longer due to direction switching" -ForegroundColor Gray
Write-Host ""

# Open serial port
try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 10000  # 10 second timeout
    $port.WriteTimeout = 5000
    $port.Open()
    Write-Host "✓ Serial port opened" -ForegroundColor Green
} catch {
    Write-Host "✗ Failed to open serial port: $_" -ForegroundColor Red
    exit 1
}

function Send-Command {
    param([string]$cmd, [int]$waitMs = 1000)
    
    Write-Host ""
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
    Write-Host "→ Sending: $cmd" -ForegroundColor Cyan
    
    # Clear buffers
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 100
    
    # Send command with CRLF
    $cmdLine = $cmd + "`r`n"
    $port.Write($cmdLine)
    
    Write-Host "  Waiting for response (up to $waitMs ms)..." -ForegroundColor Gray
    
    # Wait longer for RS-485 direction switching
    # Firmware: sets DE/RE HIGH, sends response, sets DE/RE LOW
    # This takes time!
    Start-Sleep -Milliseconds 200  # Initial delay for direction switch
    
    # Read response with longer timeout
    $response = ""
    $startTime = Get-Date
    $timeout = $waitMs
    
    while (((Get-Date) - $startTime).TotalMilliseconds -lt $timeout) {
        if ($port.BytesToRead -gt 0) {
            $newData = $port.ReadExisting()
            $response += $newData
            
            # Check if we got a complete line (ends with \r\n)
            if ($response -match "`r`n$") {
                # Got complete response
                break
            }
            
            # Small delay to see if more data is coming
            Start-Sleep -Milliseconds 50
        } else {
            # If we got some data, wait a bit more
            if ($response.Length -gt 0) {
                Start-Sleep -Milliseconds 100
                if ($port.BytesToRead -eq 0) {
                    break
                }
            }
        }
        Start-Sleep -Milliseconds 20
    }
    
    # Clean up response
    $response = $response.Trim()
    $response = $response -replace "`r`n", " "
    $response = $response.Trim()
    
    # Remove command echo if present
    if ($response -match "^$cmd") {
        $response = $response -replace "^$cmd\s*", ""
        $response = $response.Trim()
    }
    
    if ($response) {
        $elapsed = [math]::Round(((Get-Date) - $startTime).TotalMilliseconds)
        Write-Host "← Response ($elapsed ms): '$response'" -ForegroundColor $(if ($response -match "^OK") { "Green" } elseif ($response -match "^ERR") { "Red" } else { "Yellow" })
        return $response
    } else {
        $elapsed = [math]::Round(((Get-Date) - $startTime).TotalMilliseconds)
        Write-Host "← No response after $elapsed ms" -ForegroundColor Red
        return ""
    }
}

Write-Host "=== Testing Commands ===" -ForegroundColor Magenta
Write-Host ""

# Test 1: Ping
Write-Host "Test 1: PNG (Ping)" -ForegroundColor Cyan
$ping = Send-Command "PNG" 2000
if ($ping -match "^OK") {
    Write-Host "  ✓ RS-485 communication working!" -ForegroundColor Green
} else {
    Write-Host "  ✗ No valid response" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Troubleshooting:" -ForegroundColor Yellow
    Write-Host "    - Check if LED blinks when command is sent (orange LED)" -ForegroundColor White
    Write-Host "    - Check if green LED blinks (response being sent)" -ForegroundColor White
    Write-Host "    - Verify RS-485 wiring (A+, B-, GND, DE/RE pin)" -ForegroundColor White
    Write-Host "    - Check baud rate (115200)" -ForegroundColor White
    $port.Close()
    exit 1
}

# Test 2: Node Type
Write-Host ""
Write-Host "Test 2: NODE_TYPE" -ForegroundColor Cyan
$nodeType = Send-Command "NODE_TYPE" 2000
if ($nodeType -match "TX") {
    Write-Host "  ✓ This is a TX node" -ForegroundColor Green
    $isTX = $true
} elseif ($nodeType -match "RX") {
    Write-Host "  ✓ This is an RX node" -ForegroundColor Green
    $isTX = $false
} else {
    Write-Host "  ⚠ Could not determine node type" -ForegroundColor Yellow
    $isTX = $null
}

# Test 3: Check current statistics
Write-Host ""
Write-Host "Test 3: STAT (Current Statistics)" -ForegroundColor Cyan
$stats = Send-Command "STAT" 2000

if ($stats) {
    Write-Host "  Current status:" -ForegroundColor Cyan
    if ($isTX) {
        if ($stats -match "sent=(\d+)") {
            $sent = [int]$matches[1]
            Write-Host "    Packets sent: $sent" -ForegroundColor $(if ($sent -gt 0) { "Green" } else { "Yellow" })
        }
        if ($stats -match "attempted=(\d+)") {
            $attempted = [int]$matches[1]
            Write-Host "    Attempts: $attempted" -ForegroundColor $(if ($attempted -gt 0) { "Green" } else { "Yellow" })
            if ($attempted -eq 0) {
                Write-Host "    ⚠ Timer not firing - check if START was sent" -ForegroundColor Yellow
            }
        }
        if ($stats -match "errors=(\d+)") {
            $errors = [int]$matches[1]
            if ($errors -gt 0) {
                Write-Host "    Errors: $errors" -ForegroundColor Red
            }
        }
    } else {
        if ($stats -match "rx=(\d+)") {
            $rx = [int]$matches[1]
            Write-Host "    Packets received: $rx" -ForegroundColor $(if ($rx -gt 0) { "Green" } else { "Yellow" })
            if ($rx -eq 0) {
                Write-Host "    ⚠ No packets received - check TX node and configuration" -ForegroundColor Yellow
            }
        }
    }
}

# Test 4: Try to configure and start
Write-Host ""
Write-Host "Test 4: Configuration and Start" -ForegroundColor Cyan
Write-Host "  Attempting to configure UWB..." -ForegroundColor Gray

if ($isTX) {
    $config = Send-Command "CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100" 3000
} else {
    $config = Send-Command "CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100" 3000
}

if ($config -match "^OK CONFIG") {
    Write-Host "  ✓ Configuration successful" -ForegroundColor Green
    
    Write-Host "  Attempting to start test..." -ForegroundColor Gray
    $start = Send-Command "STRT" 3000
    
    if ($start -match "^OK START") {
        Write-Host "  ✓ Test started successfully" -ForegroundColor Green
        
        Write-Host "  Waiting 3 seconds for packets..." -ForegroundColor Gray
        Start-Sleep -Seconds 3
        
        $stats2 = Send-Command "STAT" 2000
        if ($stats2) {
            Write-Host ""
            Write-Host "  Updated statistics:" -ForegroundColor Cyan
            if ($isTX) {
                if ($stats2 -match "sent=(\d+)") {
                    $sent2 = [int]$matches[1]
                    Write-Host "    Packets sent: $sent2" -ForegroundColor $(if ($sent2 -gt 0) { "Green" } else { "Red" })
                }
                if ($stats2 -match "attempted=(\d+)") {
                    $attempted2 = [int]$matches[1]
                    Write-Host "    Attempts: $attempted2" -ForegroundColor $(if ($attempted2 -gt 0) { "Green" } else { "Red" })
                }
            } else {
                if ($stats2 -match "rx=(\d+)") {
                    $rx2 = [int]$matches[1]
                    Write-Host "    Packets received: $rx2" -ForegroundColor $(if ($rx2 -gt 0) { "Green" } else { "Red" })
                }
            }
        }
    } elseif ($start -match "ERR TIMER_NOT_INIT") {
        Write-Host "  ✗ Timer not initialized - firmware issue" -ForegroundColor Red
    } elseif ($start -match "ERR NOT_CONFIGURED") {
        Write-Host "  ✗ Not configured - configuration failed" -ForegroundColor Red
    } else {
        Write-Host "  ⚠ Unexpected start response: $start" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ⚠ Configuration response: $config" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Magenta
Write-Host "RS-485 Communication: $(if ($ping -match '^OK') { '✓ Working' } else { '✗ Failed' })" -ForegroundColor $(if ($ping -match '^OK') { "Green" } else { "Red" })
Write-Host "Node Type: $(if ($nodeType) { $nodeType } else { 'Unknown' })" -ForegroundColor $(if ($nodeType) { "Green" } else { "Yellow" })

$port.Close()
Write-Host ""
Write-Host "Test complete!" -ForegroundColor Green

