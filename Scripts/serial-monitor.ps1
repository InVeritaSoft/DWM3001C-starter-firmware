# Serial Monitor for Windows PowerShell
# Usage: .\serial-monitor.ps1 COM3

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "Serial Monitor - Real-time Firmware Output" -ForegroundColor Cyan
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "Port: $ComPort" -ForegroundColor White
Write-Host "Baud: 115200" -ForegroundColor White
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "Press Ctrl+C to exit" -ForegroundColor Yellow
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 1000
    $port.WriteTimeout = 1000
    $port.Open()
    
    Write-Host "✓ Connected to $ComPort" -ForegroundColor Green
    Write-Host ""
    Write-Host "Waiting for data... (send commands from another terminal)" -ForegroundColor Gray
    Write-Host ""
    
    try {
        while ($true) {
            if ($port.BytesToRead -gt 0) {
                $data = $port.ReadExisting()
                Write-Host $data -NoNewline
            } else {
                Start-Sleep -Milliseconds 100
            }
        }
    } catch [System.OperationCanceledException] {
        Write-Host ""
        Write-Host ""
        Write-Host "Monitor stopped by user" -ForegroundColor Yellow
    }
    
    $port.Close()
    
} catch {
    Write-Host "✗ Serial error: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Check COM port number (Device Manager)" -ForegroundColor White
    Write-Host "  2. Make sure no other program is using the port" -ForegroundColor White
    Write-Host "  3. Verify device is connected" -ForegroundColor White
    Write-Host ""
    Write-Host "To find your COM port:" -ForegroundColor Cyan
    Write-Host "  Get-WmiObject Win32_SerialPort | Select-Object DeviceID, Description" -ForegroundColor Gray
    exit 1
}
