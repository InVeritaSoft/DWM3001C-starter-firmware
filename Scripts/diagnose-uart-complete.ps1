# Complete UART Diagnosis Script
# Usage: .\diagnose-uart-complete.ps1 COM15

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Complete UART Diagnosis" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 5000
    $port.WriteTimeout = 2000
    $port.Open()
    
    Write-Host "✓ Connected to $ComPort" -ForegroundColor Green
    Write-Host ""
    
    # Clear buffers
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 500
    
    Write-Host "STEP 1: Checking for firmware startup messages..." -ForegroundColor Yellow
    Write-Host "  (Unplug and replug the device NOW, then wait 3 seconds)" -ForegroundColor Gray
    Start-Sleep -Seconds 3
    
    $startup = ""
    $readTimeout = 20  # 20 * 100ms = 2 seconds
    while ($readTimeout -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $startup += $port.ReadExisting()
            Start-Sleep -Milliseconds 100
        } else {
            Start-Sleep -Milliseconds 100
            $readTimeout--
        }
    }
    
    if ($startup) {
        Write-Host "✓ Startup messages received:" -ForegroundColor Green
        Write-Host $startup -ForegroundColor White
        Write-Host ""
    } else {
        Write-Host "✗ NO startup messages!" -ForegroundColor Red
        Write-Host "  This means:" -ForegroundColor Yellow
        Write-Host "    - Firmware might not be running" -ForegroundColor White
        Write-Host "    - Wrong COM port" -ForegroundColor White
        Write-Host "    - UART TX not working" -ForegroundColor White
        Write-Host ""
    }
    
    Write-Host "STEP 2: Testing UART RX (command reception)..." -ForegroundColor Yellow
    Write-Host "  WATCH THE BOARD: Orange LED should blink when command is sent!" -ForegroundColor Cyan
    Write-Host ""
    
    # Clear buffer
    $port.DiscardInBuffer()
    Start-Sleep -Milliseconds 200
    
    # Send command
    Write-Host "Sending PNG command..." -ForegroundColor Gray
    $port.WriteLine("PNG")
    
    Write-Host "Waiting for response (5 seconds)..." -ForegroundColor Gray
    Start-Sleep -Milliseconds 500
    
    $response = ""
    $timeout = 50
    while ($timeout -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $response += $port.ReadExisting()
            if ($response -match "OK|ERR") {
                break
            }
        }
        Start-Sleep -Milliseconds 100
        $timeout--
    }
    
    Write-Host ""
    if ($response) {
        $response = $response.Trim()
        Write-Host "✓ Response received: $response" -ForegroundColor Green
    } else {
        Write-Host "✗ No response received" -ForegroundColor Red
    }
    
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Diagnosis Summary" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    if (-not $startup) {
        Write-Host "❌ CRITICAL: No startup messages" -ForegroundColor Red
        Write-Host "   → Firmware might not be running" -ForegroundColor Yellow
        Write-Host "   → Check if firmware is flashed correctly" -ForegroundColor Yellow
        Write-Host "   → Verify COM port is correct" -ForegroundColor Yellow
    } elseif (-not $response) {
        Write-Host "⚠️  PARTIAL: Startup messages OK, but no command response" -ForegroundColor Yellow
        Write-Host "   → UART TX works (startup messages sent)" -ForegroundColor Green
        Write-Host "   → UART RX might not work (no response to commands)" -ForegroundColor Red
        Write-Host ""
        Write-Host "   Did the Orange LED blink when sending PNG?" -ForegroundColor Cyan
        Write-Host "     YES → RX works, but response not sent (TX issue or command not processed)" -ForegroundColor White
        Write-Host "     NO  → RX not working (GPIO/configuration issue)" -ForegroundColor White
    } else {
        Write-Host "✅ SUCCESS: Both startup and command response work!" -ForegroundColor Green
        Write-Host "   → UART TX works" -ForegroundColor Green
        Write-Host "   → UART RX works" -ForegroundColor Green
        Write-Host "   → Command processing works" -ForegroundColor Green
    }
    
    $port.Close()
    
} catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
    exit 1
}
