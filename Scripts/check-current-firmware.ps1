# Check what firmware is currently running without reflashing
# Usage: .\check-current-firmware.ps1

$ErrorActionPreference = "Stop"

Write-Host "=== Check Current Firmware Status ===" -ForegroundColor Cyan
Write-Host ""

# Check RTT logs to see what's running
Write-Host "Step 1: Checking RTT Debug Logs" -ForegroundColor Magenta
Write-Host "  This shows what firmware is actually running..." -ForegroundColor Gray
Write-Host ""

$rttFile = "Output\debug-log.txt"
if (Test-Path $rttFile) {
    $content = Get-Content $rttFile -Tail 20 -ErrorAction SilentlyContinue
    if ($content) {
        Write-Host "  Recent RTT messages:" -ForegroundColor Cyan
        $content | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        
        if ($content -match "ORCHESTRATOR|orchestrator") {
            Write-Host ""
            Write-Host "  ✓ Orchestrator firmware detected" -ForegroundColor Green
        } elseif ($content -match "SIMPLE TX|SIMPLE RX|simple") {
            Write-Host ""
            Write-Host "  ⚠ Simple example firmware detected (no RS-485 support)" -ForegroundColor Yellow
            Write-Host "    You need orchestrator firmware for Putty to work" -ForegroundColor Yellow
        } else {
            Write-Host ""
            Write-Host "  ? Could not determine firmware type" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ⚠ RTT log file is empty" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ⚠ RTT log file not found" -ForegroundColor Yellow
    Write-Host "    Run: .\stream-debug-logs.ps1" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Step 2: Check Current Configuration Files" -ForegroundColor Magenta

# Check example_selection.h
$exampleFile = "Src\example_selection.h"
if (Test-Path $exampleFile) {
    $content = Get-Content $exampleFile -Raw
    if ($content -match "#define TEST_ORCHESTRATOR_TX_V2") {
        Write-Host "  ✓ example_selection.h: TEST_ORCHESTRATOR_TX_V2 enabled" -ForegroundColor Green
    } elseif ($content -match "#define TEST_ORCHESTRATOR_RX_V2") {
        Write-Host "  ✓ example_selection.h: TEST_ORCHESTRATOR_RX_V2 enabled" -ForegroundColor Green
    } elseif ($content -match "#define TEST_SIMPLE_TX") {
        Write-Host "  ⚠ example_selection.h: TEST_SIMPLE_TX enabled (no RS-485)" -ForegroundColor Yellow
    } elseif ($content -match "#define TEST_SIMPLE_RX") {
        Write-Host "  ⚠ example_selection.h: TEST_SIMPLE_RX enabled (no RS-485)" -ForegroundColor Yellow
    } else {
        Write-Host "  ? example_selection.h: Could not determine active example" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ✗ example_selection.h not found" -ForegroundColor Red
}

Write-Host ""
Write-Host "Step 3: Test Serial Communication" -ForegroundColor Magenta
Write-Host "  Which COM port are you using in Putty?" -ForegroundColor Cyan
$comPort = Read-Host "  Enter COM port (or press Enter to skip)"

if ($comPort) {
    Write-Host ""
    Write-Host "  Testing $comPort..." -ForegroundColor Gray
    
    try {
        $port = New-Object System.IO.Ports.SerialPort $comPort, 115200, None, 8, one
        $port.ReadTimeout = 3000
        $port.WriteTimeout = 3000
        $port.Open()
        
        Write-Host "  ✓ Port opened" -ForegroundColor Green
        
        # Clear buffers
        $port.DiscardInBuffer()
        $port.DiscardOutBuffer()
        Start-Sleep -Milliseconds 500
        
        # Try to read any existing data (startup messages)
        Start-Sleep -Milliseconds 200
        $existing = ""
        if ($port.BytesToRead -gt 0) {
            $existing = $port.ReadExisting()
            if ($existing) {
                Write-Host "  ✓ Found data in buffer: '$($existing.Trim())'" -ForegroundColor Green
            }
        }
        
        # Send PNG command
        Write-Host "  Sending PNG command..." -ForegroundColor Gray
        $port.WriteLine("PNG")
        
        # Wait for response
        Start-Sleep -Milliseconds 1000
        
        $response = ""
        $timeout = 100
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
            Write-Host "  ✓ Response: '$response'" -ForegroundColor Green
            
            if ($response -match "^OK") {
                Write-Host ""
                Write-Host "  ✓✓✓ COMMUNICATION WORKING! ✓✓✓" -ForegroundColor Green
                Write-Host "  Putty should work. Check:" -ForegroundColor Cyan
                Write-Host "    - Baud rate: 115200" -ForegroundColor White
                Write-Host "    - Flow control: None" -ForegroundColor White
            }
        } else {
            Write-Host "  ✗ No response" -ForegroundColor Red
            
            if ($existing) {
                Write-Host ""
                Write-Host "  Note: Found startup messages but no command response" -ForegroundColor Yellow
                Write-Host "  This suggests:" -ForegroundColor Yellow
                Write-Host "    - Firmware is running" -ForegroundColor White
                Write-Host "    - UART TX works (startup messages)" -ForegroundColor White
                Write-Host "    - But command processing might not work" -ForegroundColor White
            } else {
                Write-Host ""
                Write-Host "  No data received at all" -ForegroundColor Yellow
                Write-Host "  Possible issues:" -ForegroundColor Yellow
                Write-Host "    - Wrong COM port" -ForegroundColor White
                Write-Host "    - Wrong baud rate (should be 115200)" -ForegroundColor White
                Write-Host "    - RS-485 hardware not connected" -ForegroundColor White
                Write-Host "    - Firmware not running (check RTT logs)" -ForegroundColor White
            }
        }
        
        $port.Close()
    } catch {
        Write-Host "  ✗ Error: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "  Possible issues:" -ForegroundColor Yellow
        Write-Host "    - Port already open (close Putty first)" -ForegroundColor White
        Write-Host "    - Wrong COM port" -ForegroundColor White
        Write-Host "    - Port doesn't exist" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "To see what firmware is running:" -ForegroundColor Yellow
Write-Host "  .\stream-debug-logs.ps1" -ForegroundColor White
Write-Host ""
Write-Host "To test serial communication:" -ForegroundColor Yellow
Write-Host "  .\test-rs485-communication.ps1 COM11" -ForegroundColor White
Write-Host ""
Write-Host "If Putty still doesn't work:" -ForegroundColor Yellow
Write-Host "  1. Check Putty settings (115200, 8N1, no flow control)" -ForegroundColor White
Write-Host "  2. Verify COM port in Device Manager" -ForegroundColor White
Write-Host "  3. Check RS-485 hardware connections" -ForegroundColor White
Write-Host "  4. Try resetting the board" -ForegroundColor White

