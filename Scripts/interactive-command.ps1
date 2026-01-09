# Interactive Command Sender
# Usage: .\interactive-command.ps1 COM15

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=== Interactive Command Sender ===" -ForegroundColor Cyan
Write-Host "Port: $ComPort" -ForegroundColor White
Write-Host "Baud: 115200" -ForegroundColor White
Write-Host "Type 'quit' or 'exit' to stop" -ForegroundColor Yellow
Write-Host ""

try {
    $port = New-Object System.IO.Ports.SerialPort $ComPort, 115200, None, 8, one
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    $port.Open()
    
    Write-Host "✓ Connected to $ComPort" -ForegroundColor Green
    Write-Host ""
    
    while ($true) {
        $cmd = Read-Host "Command"
        
        if ($cmd -eq 'quit' -or $cmd -eq 'exit' -or $cmd -eq 'q') {
            break
        }
        
        if ([string]::IsNullOrWhiteSpace($cmd)) {
            continue
        }
        
        # Clear input buffer
        $port.DiscardInBuffer()
        Start-Sleep -Milliseconds 100
        
        # Send command
        $port.WriteLine($cmd)
        Write-Host "→ Sent: $cmd" -ForegroundColor Gray
        
        # Wait for response
        Start-Sleep -Milliseconds 500
        
        $response = ""
        $timeout = 20
        while ($timeout -gt 0) {
            if ($port.BytesToRead -gt 0) {
                $response += $port.ReadExisting()
                if ($response -match "OK|ERR|\[") {
                    break
                }
            }
            Start-Sleep -Milliseconds 100
            $timeout--
        }
        
        if ($response) {
            $response = $response.Trim()
            Write-Host "← Response: $response" -ForegroundColor Green
        } else {
            Write-Host "← No response" -ForegroundColor Yellow
        }
        
        Write-Host ""
    }
    
    $port.Close()
    Write-Host "Disconnected" -ForegroundColor Yellow
    
} catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
    exit 1
}
