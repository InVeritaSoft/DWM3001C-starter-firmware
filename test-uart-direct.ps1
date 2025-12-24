# Direct UART test script
# Tests if boards are responding via COM ports
# Usage: .\test-uart-direct.ps1

Write-Host "=== Direct UART Test ===" -ForegroundColor Cyan
Write-Host ""

$comPorts = @("COM17", "COM18")

foreach ($port in $comPorts) {
    Write-Host "Testing $port..." -ForegroundColor Yellow
    
    try {
        # Try to open port
        $portObj = New-Object System.IO.Ports.SerialPort $port, 115200, None, 8, One
        $portObj.ReadTimeout = 2000
        $portObj.WriteTimeout = 2000
        $portObj.Open()
        
        Write-Host "  Port opened successfully" -ForegroundColor Green
        
        # Clear any existing data
        $portObj.DiscardInBuffer()
        $portObj.DiscardOutBuffer()
        
        # First, check if there's any data already waiting (startup messages)
        Write-Host "  Checking for startup messages (waiting 2 seconds)..." -ForegroundColor Gray
        $startupData = ""
        $startupStartTime = Get-Date
        while ((Get-Date) -lt $startupStartTime.AddSeconds(2)) {
            if ($portObj.BytesToRead -gt 0) {
                $startupData += $portObj.ReadExisting()
            }
            Start-Sleep -Milliseconds 100
        }
        
        if ($startupData) {
            Write-Host "  Startup data received:" -ForegroundColor Green
            Write-Host "    $startupData" -ForegroundColor White
        } else {
            Write-Host "  No startup data received" -ForegroundColor Yellow
        }
        
        # Clear buffers before sending command
        $portObj.DiscardInBuffer()
        $portObj.DiscardOutBuffer()
        Start-Sleep -Milliseconds 200
        
        # Send PING command
        Write-Host "  Sending: PNG" -ForegroundColor Cyan
        $portObj.WriteLine("PNG")
        Write-Host "  Command sent (PNG + CRLF)" -ForegroundColor Gray
        
        # Try to read ANY data (not just OK responses)
        $allData = ""
        $timeout = 3000
        $startTime = Get-Date
        $bytesRead = 0
        
        Write-Host "  Waiting for response (checking for any data, including test chars)..." -ForegroundColor Gray
        
        while ((Get-Date) -lt $startTime.AddMilliseconds($timeout)) {
            if ($portObj.BytesToRead -gt 0) {
                $data = $portObj.ReadExisting()
                $allData += $data
                $bytesRead += $data.Length
                
                # Show each chunk as it arrives
                $asciiData = [System.Text.Encoding]::ASCII.GetString([System.Text.Encoding]::ASCII.GetBytes($data))
                Write-Host "    [+$($data.Length) bytes] $asciiData" -ForegroundColor Cyan
                
                # Check for various response patterns
                if ($allData -match "OK|ERR|STARTUP|DW3000|UART|MAIN_LOOP|FIRMWARE") {
                    Write-Host "    Found diagnostic message!" -ForegroundColor Green
                    break
                }
                
                # Check for test characters
                if ($allData -match "!UART") {
                    Write-Host "    Found test characters (!UART)!" -ForegroundColor Green
                }
            }
            Start-Sleep -Milliseconds 100
        }
        
        if ($allData) {
            Write-Host "  Data received ($bytesRead bytes):" -ForegroundColor Green
            Write-Host "    $allData" -ForegroundColor White
            
            if ($allData -match "OK") {
                Write-Host "  ✓ Valid response received!" -ForegroundColor Green
            } elseif ($allData -match "ERR") {
                Write-Host "  ⚠ Error response received" -ForegroundColor Yellow
            } else {
                Write-Host "  ? Unknown data received (may be firmware debug output)" -ForegroundColor Yellow
            }
        } else {
            Write-Host "  ✗ No data received at all" -ForegroundColor Red
            Write-Host "    Possible issues:" -ForegroundColor Yellow
            Write-Host "    1. Board not powered on" -ForegroundColor White
            Write-Host "    2. Wrong COM port (check Device Manager)" -ForegroundColor White
            Write-Host "    3. RS-485 wiring incorrect (A+, B-, GND)" -ForegroundColor White
            Write-Host "    4. Firmware not running (may have crashed during init)" -ForegroundColor White
            Write-Host "    5. Firmware not flashed correctly" -ForegroundColor White
        }
        
        $portObj.Close()
    }
    catch {
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "    Port may be in use or not available" -ForegroundColor Yellow
    }
    
    Write-Host ""
}

Write-Host "=== Test Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "If no responses:" -ForegroundColor Yellow
Write-Host "  1. Verify boards are powered on (LEDs should blink on startup)" -ForegroundColor White
Write-Host "  2. Check RS-485 wiring:" -ForegroundColor White
Write-Host "     - A+ to A+ (all devices)" -ForegroundColor Gray
Write-Host "     - B- to B- (all devices)" -ForegroundColor Gray
Write-Host "     - GND to GND (all devices)" -ForegroundColor Gray
Write-Host "  3. Verify firmware was flashed correctly:" -ForegroundColor White
Write-Host "     - Run: .\build-and-flash-tx.ps1" -ForegroundColor Gray
Write-Host "     - Run: .\build-and-flash-rx.ps1" -ForegroundColor Gray
Write-Host "  4. Check COM ports match settings.yaml (COM17, COM18)" -ForegroundColor White
