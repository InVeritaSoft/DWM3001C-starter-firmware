# Debug serial communication
# Usage: .\serial-debug.ps1 COM11 NODE_TYPE

param(
    [string]$ComPort = "COM11",
    [string]$Command = "PNG"
)

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, "None", 8, "One"
    $port.ReadTimeout = 5000
    $port.WriteTimeout = 2000
    $port.DtrEnable = $false
    $port.RtsEnable = $false
    $port.Open()
    
    Write-Host "Connected to $ComPort at 115200 baud" -ForegroundColor Green
    Write-Host ""
    
    # Clear buffers
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 200
    
    # Check if there's any initial data
    if ($port.BytesToRead -gt 0) {
        $initial = $port.ReadExisting()
        Write-Host "Initial data: [$initial]" -ForegroundColor Gray
    }
    
    Write-Host "TX: $Command" -ForegroundColor Yellow
    # Send with just CR (no LF) - matches how many serial terminals work
    $port.Write("$Command`r")
    
    Write-Host "Waiting for response..." -ForegroundColor Gray
    Start-Sleep -Milliseconds 1000
    
    $response = ""
    $maxWait = 50
    while ($maxWait -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $data = $port.ReadExisting()
            $response += $data
            Write-Host "  Received $($data.Length) bytes" -ForegroundColor DarkGray
        }
        Start-Sleep -Milliseconds 100
        $maxWait--
        
        # Break if we got a full response
        if ($response -match "`r`n$|`n$") {
            break
        }
    }
    
    Write-Host ""
    if ($response) {
        $trimmed = $response.Trim()
        $hexBytes = [System.BitConverter]::ToString([System.Text.Encoding]::ASCII.GetBytes($response))
        Write-Host "RX (raw hex): $hexBytes" -ForegroundColor Gray
        Write-Host "RX (text): $trimmed" -ForegroundColor Cyan
    } else {
        Write-Host "No response received!" -ForegroundColor Red
    }
    
    $port.Close()
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    if ($port -and $port.IsOpen) { $port.Close() }
    exit 1
}
