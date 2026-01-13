# Test multiple commands sequentially
param([string]$ComPort = "COM11")

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, "None", 8, "One"
    $port.ReadTimeout = 3000
    $port.WriteTimeout = 2000
    $port.DtrEnable = $false
    $port.RtsEnable = $false
    $port.Open()
    
    Write-Host "Connected to $ComPort" -ForegroundColor Green
    
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 500

    function Send-Command {
        param([string]$Cmd)
        
        # Clear any pending data
        if ($port.BytesToRead -gt 0) {
            $null = $port.ReadExisting()
        }
        
        Write-Host -NoNewline "TX: $Cmd -> " -ForegroundColor Yellow
        $port.Write("$Cmd`r")
        
        Start-Sleep -Milliseconds 500
        
        $response = ""
        $maxWait = 30
        while ($maxWait -gt 0) {
            if ($port.BytesToRead -gt 0) {
                $response += $port.ReadExisting()
            }
            Start-Sleep -Milliseconds 50
            $maxWait--
            
            if ($response -match "`r`n|`n") {
                break
            }
        }
        
        if ($response) {
            $trimmed = $response.Trim()
            $color = if ($trimmed -match "^OK") { "Green" } elseif ($trimmed -match "^ERR") { "Red" } else { "Gray" }
            Write-Host "RX: $trimmed" -ForegroundColor $color
        } else {
            Write-Host "RX: (no response)" -ForegroundColor Red
        }
        
        Start-Sleep -Milliseconds 200
    }

    Write-Host ""
    Send-Command "PNG"
    Send-Command "PING"
    Send-Command "STAT"
    Send-Command "RST"
    Send-Command "NODE_TYPE"
    Send-Command "STOP"
    
    $port.Close()
    Write-Host ""
    Write-Host "Done" -ForegroundColor Cyan
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    if ($port -and $port.IsOpen) { $port.Close() }
    exit 1
}
