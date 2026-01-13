# Test NODE_TYPE specifically
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
    Start-Sleep -Milliseconds 300

    function Send-Command {
        param([string]$Cmd)
        
        if ($port.BytesToRead -gt 0) {
            $null = $port.ReadExisting()
        }
        
        Write-Host -NoNewline "TX: $Cmd -> " -ForegroundColor Yellow
        $port.Write("$Cmd`r")
        
        Start-Sleep -Milliseconds 800
        
        $response = ""
        $maxWait = 40
        while ($maxWait -gt 0) {
            if ($port.BytesToRead -gt 0) {
                $data = $port.ReadExisting()
                $response += $data
                Write-Host -NoNewline "." -ForegroundColor Gray
            }
            Start-Sleep -Milliseconds 50
            $maxWait--
            
            if ($response -match "`r`n|`n") {
                break
            }
        }
        
        Write-Host ""
        if ($response) {
            $trimmed = $response.Trim()
            $color = if ($trimmed -match "^OK") { "Green" } elseif ($trimmed -match "^ERR") { "Red" } else { "Gray" }
            Write-Host "  RX: $trimmed" -ForegroundColor $color
            return $true
        } else {
            Write-Host "  RX: (no response)" -ForegroundColor Red
            return $false
        }
    }

    Write-Host ""
    Write-Host "Test 1: PING before NODE_TYPE" -ForegroundColor Cyan
    Send-Command "PING"
    
    Write-Host ""
    Write-Host "Test 2: NODE_TYPE" -ForegroundColor Cyan
    Send-Command "NODE_TYPE"
    
    Write-Host ""
    Write-Host "Test 3: PING after NODE_TYPE (check if firmware is still running)" -ForegroundColor Cyan
    Send-Command "PING"
    
    Write-Host ""
    Write-Host "Test 4: STAT (longer response)" -ForegroundColor Cyan
    Send-Command "STAT"
    
    $port.Close()
    Write-Host ""
    Write-Host "Done" -ForegroundColor Cyan
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    if ($port -and $port.IsOpen) { $port.Close() }
    exit 1
}
