# Raw serial debug tool - shows exactly what's being sent and received
# Usage: .\debug-serial-raw.ps1 <COM_PORT>
# Example: .\debug-serial-raw.ps1 COM11

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=== Raw Serial Debug Tool ===" -ForegroundColor Cyan
Write-Host "COM Port: $ComPort" -ForegroundColor Yellow
Write-Host "This tool shows raw bytes being sent/received" -ForegroundColor Gray
Write-Host "Press Ctrl+C to exit" -ForegroundColor Gray
Write-Host ""

# Open serial port
try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 5000
    $port.WriteTimeout = 5000
    $port.Open()
    Write-Host "✓ Serial port opened" -ForegroundColor Green
} catch {
    Write-Host "✗ Failed to open serial port: $_" -ForegroundColor Red
    exit 1
}

function Show-Hex {
    param([byte[]]$bytes)
    if ($bytes.Length -eq 0) { return "" }
    return ($bytes | ForEach-Object { "{0:X2}" -f $_ }) -join " "
}

function Show-Ascii {
    param([byte[]]$bytes)
    if ($bytes.Length -eq 0) { return "" }
    return [System.Text.Encoding]::ASCII.GetString($bytes)
}

function Send-Command-Raw {
    param([string]$cmd)
    
    Write-Host ""
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
    Write-Host "→ Sending command: '$cmd'" -ForegroundColor Cyan
    
    # Clear buffers
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    Start-Sleep -Milliseconds 100
    
    # Send command
    $cmdBytes = [System.Text.Encoding]::ASCII.GetBytes($cmd + "`r`n")
    $port.Write($cmdBytes, 0, $cmdBytes.Length)
    
    Write-Host "  Sent bytes: $(Show-Hex $cmdBytes)" -ForegroundColor Gray
    Write-Host "  Sent ASCII: $(Show-Ascii $cmdBytes)" -ForegroundColor Gray
    
    # Wait a bit
    Start-Sleep -Milliseconds 500
    
    # Read all available data
    $allData = New-Object System.Collections.ArrayList
    $timeout = 100  # 100 * 50ms = 5 seconds
    
    Write-Host ""
    Write-Host "  Reading response..." -ForegroundColor Gray
    
    while ($timeout -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $bytes = New-Object byte[] $port.BytesToRead
            $bytesRead = $port.Read($bytes, 0, $bytes.Length)
            if ($bytesRead -gt 0) {
                $actualBytes = $bytes[0..($bytesRead-1)]
                [void]$allData.AddRange($actualBytes)
                Write-Host "    Read $bytesRead bytes: $(Show-Hex $actualBytes)" -ForegroundColor DarkGray
            }
            # Small delay to see if more data is coming
            Start-Sleep -Milliseconds 50
        } else {
            # If we got data and nothing more is coming, break
            if ($allData.Count -gt 0) {
                Start-Sleep -Milliseconds 100
                if ($port.BytesToRead -eq 0) {
                    break
                }
            }
        }
        Start-Sleep -Milliseconds 50
        $timeout--
    }
    
    if ($allData.Count -gt 0) {
        $responseBytes = $allData.ToArray()
        $responseHex = Show-Hex $responseBytes
        $responseAscii = Show-Ascii $responseBytes
        
        Write-Host ""
        Write-Host "← Response received:" -ForegroundColor Green
        Write-Host "  Hex:    $responseHex" -ForegroundColor Yellow
        Write-Host "  ASCII:  $responseAscii" -ForegroundColor Yellow
        Write-Host "  Length: $($responseBytes.Length) bytes" -ForegroundColor Yellow
        
        # Try to parse as string
        $responseStr = $responseAscii.Trim()
        Write-Host ""
        Write-Host "  Parsed: '$responseStr'" -ForegroundColor $(if ($responseStr -match "^OK") { "Green" } elseif ($responseStr -match "^ERR") { "Red" } else { "Yellow" })
        
        return $responseStr
    } else {
        Write-Host ""
        Write-Host "← No response received (timeout)" -ForegroundColor Red
        return ""
    }
}

Write-Host "=== Testing Basic Commands ===" -ForegroundColor Magenta
Write-Host ""

# Test 1: Ping
Write-Host "Test 1: PNG (Ping)" -ForegroundColor Cyan
$ping = Send-Command-Raw "PNG"

# Test 2: Node Type
Write-Host ""
Write-Host "Test 2: NODE_TYPE" -ForegroundColor Cyan
$nodeType = Send-Command-Raw "NODE_TYPE"

# Test 3: Statistics
Write-Host ""
Write-Host "Test 3: STAT (Statistics)" -ForegroundColor Cyan
$stats = Send-Command-Raw "STAT"

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Magenta
Write-Host "PNG response:      $(if ($ping) { "'$ping'" } else { "(none)" })" -ForegroundColor $(if ($ping -match "^OK") { "Green" } elseif ($ping) { "Yellow" } else { "Red" })
Write-Host "NODE_TYPE response: $(if ($nodeType) { "'$nodeType'" } else { "(none)" })" -ForegroundColor $(if ($nodeType) { "Green" } else { "Red" })
Write-Host "STAT response:     $(if ($stats) { "'$stats'" } else { "(none)" })" -ForegroundColor $(if ($stats) { "Green" } else { "Red" })

Write-Host ""
Write-Host "=== Interactive Mode ===" -ForegroundColor Magenta
Write-Host "Type commands to send (or 'quit' to exit):" -ForegroundColor Cyan
Write-Host ""

while ($true) {
    Write-Host "Command> " -NoNewline -ForegroundColor Cyan
    $cmd = Read-Host
    
    if ($cmd -eq "quit" -or $cmd -eq "exit" -or $cmd -eq "q") {
        break
    }
    
    if ($cmd.Length -gt 0) {
        Send-Command-Raw $cmd
    }
}

Write-Host ""
Write-Host "Closing port..." -ForegroundColor Gray
$port.Close()
Write-Host "Done!" -ForegroundColor Green

