# Diagnose why Putty isn't responding after flashing orchestrator firmware
# Usage: .\diagnose-putty-no-response.ps1

$ErrorActionPreference = "Stop"

Write-Host "=== Putty No Response Diagnostic ===" -ForegroundColor Cyan
Write-Host ""

# Step 1: Check if firmware is running (RTT logs)
Write-Host "Step 1: Check RTT Debug Logs" -ForegroundColor Magenta
Write-Host "  This will show if firmware is actually running..." -ForegroundColor Gray
Write-Host ""

$rttRunning = $false
try {
    $rttProcess = Start-Process -FilePath "pwsh" -ArgumentList "-File", ".\stream-debug-logs.ps1" -PassThru -NoNewWindow -RedirectStandardOutput "rtt-output.txt" -RedirectStandardError "rtt-error.txt"
    Start-Sleep -Seconds 3
    
    if (Test-Path "rtt-output.txt") {
        $rttContent = Get-Content "rtt-output.txt" -ErrorAction SilentlyContinue
        if ($rttContent -match "ORCHESTRATOR|STARTUP|UART|DW3000") {
            Write-Host "  ✓ Firmware appears to be running (found firmware messages)" -ForegroundColor Green
            Write-Host "    Recent messages:" -ForegroundColor Gray
            $rttContent | Select-Object -Last 5 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkGray }
            $rttRunning = $true
        } else {
            Write-Host "  ⚠ No firmware messages found in RTT" -ForegroundColor Yellow
        }
    }
    
    Stop-Process -Id $rttProcess.Id -Force -ErrorAction SilentlyContinue
} catch {
    Write-Host "  ⚠ Could not check RTT logs: $_" -ForegroundColor Yellow
}

Write-Host ""

# Step 2: List available COM ports
Write-Host "Step 2: Available COM Ports" -ForegroundColor Magenta
try {
    $ports = Get-WmiObject -Class Win32_SerialPort | Select-Object DeviceID, Description, Name
    if ($ports) {
        Write-Host "  Available ports:" -ForegroundColor Cyan
        foreach ($port in $ports) {
            Write-Host "    $($port.DeviceID) - $($port.Description)" -ForegroundColor White
        }
    } else {
        Write-Host "  ⚠ No COM ports found" -ForegroundColor Yellow
    }
} catch {
    Write-Host "  ⚠ Could not list COM ports: $_" -ForegroundColor Yellow
}

Write-Host ""

# Step 3: Test serial communication
Write-Host "Step 3: Test Serial Communication" -ForegroundColor Magenta
Write-Host "  Which COM port are you using in Putty? (e.g., COM11)" -ForegroundColor Cyan
$comPort = Read-Host "  Enter COM port"

if ($comPort) {
    Write-Host ""
    Write-Host "  Testing $comPort..." -ForegroundColor Gray
    
    try {
        $port = New-Object System.IO.Ports.SerialPort $comPort, 115200, None, 8, one
        $port.ReadTimeout = 5000
        $port.WriteTimeout = 5000
        $port.Open()
        
        Write-Host "  ✓ Port opened successfully" -ForegroundColor Green
        
        # Clear buffers
        $port.DiscardInBuffer()
        $port.DiscardOutBuffer()
        Start-Sleep -Milliseconds 500
        
        # Send PNG command
        Write-Host "  Sending PNG command..." -ForegroundColor Gray
        $port.WriteLine("PNG")
        
        # Wait for response
        Start-Sleep -Milliseconds 1000
        
        $response = ""
        $timeout = 100  # 100 * 50ms = 5 seconds
        while ($timeout -gt 0) {
            if ($port.BytesToRead -gt 0) {
                $response += $port.ReadExisting()
                if ($response -match "OK|ERR") {
                    break
                }
            }
            Start-Sleep -Milliseconds 50
            $timeout--
        }
        
        if ($response) {
            $response = $response.Trim()
            Write-Host "  ✓ Response received: '$response'" -ForegroundColor Green
            
            if ($response -match "^OK") {
                Write-Host ""
                Write-Host "  ✓✓✓ COMMUNICATION WORKING! ✓✓✓" -ForegroundColor Green
                Write-Host "  Putty should work now. Make sure:" -ForegroundColor Cyan
                Write-Host "    - Baud rate: 115200" -ForegroundColor White
                Write-Host "    - Data bits: 8" -ForegroundColor White
                Write-Host "    - Stop bits: 1" -ForegroundColor White
                Write-Host "    - Parity: None" -ForegroundColor White
                Write-Host "    - Flow control: None" -ForegroundColor White
            } else {
                Write-Host "  ⚠ Unexpected response format" -ForegroundColor Yellow
            }
        } else {
            Write-Host "  ✗ No response received" -ForegroundColor Red
            Write-Host ""
            Write-Host "  Possible issues:" -ForegroundColor Yellow
            Write-Host "    1. Wrong COM port" -ForegroundColor White
            Write-Host "    2. RS-485 hardware not connected" -ForegroundColor White
            Write-Host "    3. Firmware not running (check RTT logs)" -ForegroundColor White
            Write-Host "    4. Wrong baud rate (should be 115200)" -ForegroundColor White
            Write-Host "    5. RS-485 transceiver DE/RE pin issue" -ForegroundColor White
        }
        
        $port.Close()
    } catch {
        Write-Host "  ✗ Error: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "  Possible issues:" -ForegroundColor Yellow
        Write-Host "    - Port already open (close Putty first)" -ForegroundColor White
        Write-Host "    - Wrong COM port number" -ForegroundColor White
        Write-Host "    - Port doesn't exist" -ForegroundColor White
    }
} else {
    Write-Host "  ⚠ No COM port specified" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Diagnostic Summary ===" -ForegroundColor Cyan
Write-Host "Firmware Running: $(if ($rttRunning) { '✓ Yes' } else { '? Unknown (check RTT logs)' })" -ForegroundColor $(if ($rttRunning) { "Green" } else { "Yellow" })
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Check RTT logs: .\stream-debug-logs.ps1" -ForegroundColor White
Write-Host "  2. Verify COM port in Putty matches your board" -ForegroundColor White
Write-Host "  3. Check RS-485 wiring (A+, B-, GND, DE/RE pin)" -ForegroundColor White
Write-Host "  4. Try resetting the board (press reset button)" -ForegroundColor White
Write-Host "  5. Verify baud rate is 115200 in Putty" -ForegroundColor White

