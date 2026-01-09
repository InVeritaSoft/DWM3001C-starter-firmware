# Test UART RX - Check if firmware is receiving commands
# Usage: .\test-uart-rx.ps1 COM15

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=== UART RX Test ===" -ForegroundColor Cyan
Write-Host "This test checks if the firmware is receiving commands"
Write-Host "Watch for LED activity on the board when commands are sent"
Write-Host ""

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    $port.Open()
    
    Write-Host "✓ Connected to $ComPort" -ForegroundColor Green
    Write-Host ""
    
    # Clear buffers
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 500
    
    # Check for startup messages
    Write-Host "Checking for startup messages..." -ForegroundColor Yellow
    Start-Sleep -Milliseconds 1000
    
    $startup = ""
    if ($port.BytesToRead -gt 0) {
        $startup = $port.ReadExisting()
        Write-Host "Startup messages:" -ForegroundColor Green
        Write-Host $startup
    } else {
        Write-Host "No startup messages (firmware might not be running)" -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "=" * 50 -ForegroundColor Cyan
    Write-Host "Sending PNG command..." -ForegroundColor Yellow
    Write-Host "WATCH THE BOARD: Orange LED should blink if RX is working!" -ForegroundColor Cyan
    Write-Host "=" * 50 -ForegroundColor Cyan
    Write-Host ""
    
    # Clear buffer before sending
    $port.DiscardInBuffer()
    Start-Sleep -Milliseconds 100
    
    # Send command
    $port.WriteLine("PNG")
    Write-Host "Command sent. Waiting for response..." -ForegroundColor Gray
    
    # Wait for response
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
        Start-Sleep -Milliseconds 20
        $timeout--
    }
    
    Write-Host ""
    if ($response) {
        $response = $response.Trim()
        Write-Host "✓ Response received: $response" -ForegroundColor Green
    } else {
        Write-Host "✗ No response received" -ForegroundColor Red
        Write-Host ""
        Write-Host "Diagnosis:" -ForegroundColor Yellow
        Write-Host "  - Did the Orange LED blink? (indicates RX is working)" -ForegroundColor White
        Write-Host "  - Did you see startup messages? (indicates firmware is running)" -ForegroundColor White
        Write-Host "  - If LED blinked but no response: UART RX works but TX might not" -ForegroundColor White
        Write-Host "  - If no LED blink: UART RX not working (GPIO/configuration issue)" -ForegroundColor White
    }
    
    $port.Close()
    
} catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
    exit 1
}
