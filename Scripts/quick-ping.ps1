# Quick PING test for serial ports
# Usage: .\quick-ping.ps1 COM11

param([string]$ComPort = "COM11")

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, "None", 8, "One"
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    $port.Open()
    
    Write-Host "Connected to $ComPort" -ForegroundColor Green
    
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 100
    
    Write-Host "Sending PNG..." -ForegroundColor Yellow
    $port.WriteLine("PNG")
    
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
    
    if ($response) {
        $trimmed = $response.Trim()
        $color = if ($trimmed -match "^OK") { "Green" } else { "Yellow" }
        Write-Host "Response: $trimmed" -ForegroundColor $color
    } else {
        Write-Host "No response received" -ForegroundColor Red
    }
    
    $port.Close()
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}
