# PowerShell script to send commands to serial port
# Usage: .\SEND_COMMANDS_WINDOWS.ps1 COM3 PNG

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort,
    
    [Parameter(Mandatory=$false)]
    [string]$Command = "PNG"
)

Write-Host "Connecting to $ComPort..." -ForegroundColor Cyan

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    $port.Open()
    
    Write-Host "✓ Connected" -ForegroundColor Green
    Write-Host ""
    
    # Clear buffers
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 100
    
    # Send command
    Write-Host "Sending: $Command" -ForegroundColor Yellow
    $port.WriteLine($Command)
    
    # Wait for response
    Write-Host "Waiting for response..." -ForegroundColor Gray
    Start-Sleep -Milliseconds 500
    
    $response = ""
    $timeout = 50  # 50 * 20ms = 1 second
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
    
    if ($response) {
        $response = $response.Trim()
        Write-Host "Response: $response" -ForegroundColor $(if ($response -match "^OK") { "Green" } else { "Yellow" })
    } else {
        Write-Host "No response received" -ForegroundColor Red
    }
    
    $port.Close()
    
} catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Check COM port number (Device Manager)"
    Write-Host "  2. Make sure no other program is using the port"
    Write-Host "  3. Verify device is connected"
    exit 1
}
