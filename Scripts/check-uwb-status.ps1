# Simple script to check UWB status and send commands manually
# Usage: .\check-uwb-status.ps1 <COM_PORT>
# Example: .\check-uwb-status.ps1 COM11

param(
    [Parameter(Mandatory=$true)]
    [string]$ComPort
)

$ErrorActionPreference = "Stop"

Write-Host "=== UWB Status Checker ===" -ForegroundColor Cyan
Write-Host "COM Port: $ComPort" -ForegroundColor Yellow
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

function Read-Response {
    $response = ""
    $timeout = 200  # 200 * 25ms = 5 seconds
    $lastData = ""
    
    while ($timeout -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $newData = $port.ReadExisting()
            $response += $newData
            $lastData = $newData
            # Check if we got a complete line
            if ($lastData -match "`r`n") {
                Start-Sleep -Milliseconds 50
                if ($port.BytesToRead -gt 0) {
                    $response += $port.ReadExisting()
                }
                break
            }
        } else {
            # If we got data and nothing more is coming, break
            if ($response.Length -gt 0 -and $lastData -match "`r`n") {
                break
            }
        }
        Start-Sleep -Milliseconds 25
        $timeout--
    }
    
    return $response.Trim()
}

function Send-Command {
    param([string]$cmd)
    
    Write-Host ""
    Write-Host "→ Sending: $cmd" -ForegroundColor Cyan
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()
    $port.WriteLine($cmd)
    Start-Sleep -Milliseconds 300
    
    $response = Read-Response
    
    if ($response) {
        # Remove command echo if present
        $response = $response -replace "^$cmd\s*", ""
        $response = $response.Trim()
        
        if ($response -match "^OK") {
            Write-Host "← $response" -ForegroundColor Green
        } elseif ($response -match "^ERR") {
            Write-Host "← $response" -ForegroundColor Red
        } else {
            Write-Host "← $response" -ForegroundColor Yellow
        }
    } else {
        Write-Host "← (no response)" -ForegroundColor Yellow
    }
    
    return $response
}

Write-Host "=== Quick Status Check ===" -ForegroundColor Magenta
Write-Host ""

# Check ping
$ping = Send-Command "PNG"
if ($ping -match "^OK") {
    Write-Host "✓ RS-485 communication working" -ForegroundColor Green
} else {
    Write-Host "⚠ Unexpected ping response: $ping" -ForegroundColor Yellow
}

# Check node type
$nodeType = Send-Command "NODE_TYPE"
if ($nodeType -match "TX") {
    Write-Host "✓ This is a TX node" -ForegroundColor Green
    $isTX = $true
} elseif ($nodeType -match "RX") {
    Write-Host "✓ This is an RX node" -ForegroundColor Green
    $isTX = $false
} else {
    Write-Host "⚠ Could not determine node type" -ForegroundColor Yellow
    $isTX = $null
}

# Check current statistics
Write-Host ""
Write-Host "=== Current Statistics ===" -ForegroundColor Magenta
$stats = Send-Command "STAT"

# Parse statistics
if ($isTX) {
    if ($stats -match "sent=(\d+)") {
        $sent = $matches[1]
        Write-Host "  Packets sent: $sent" -ForegroundColor $(if ([int]$sent -gt 0) { "Green" } else { "Yellow" })
    }
    if ($stats -match "attempted=(\d+)") {
        $attempted = $matches[1]
        Write-Host "  Attempts: $attempted" -ForegroundColor $(if ([int]$attempted -gt 0) { "Green" } else { "Yellow" })
    }
    if ($stats -match "errors=(\d+)") {
        $errors = $matches[1]
        if ([int]$errors -gt 0) {
            Write-Host "  Errors: $errors" -ForegroundColor Red
        }
    }
} elseif ($isTX -eq $false) {
    if ($stats -match "rx=(\d+)") {
        $rx = $matches[1]
        Write-Host "  Packets received: $rx" -ForegroundColor $(if ([int]$rx -gt 0) { "Green" } else { "Yellow" })
    }
    if ($stats -match "lost=(\d+)") {
        $lost = $matches[1]
        if ([int]$lost -gt 0) {
            Write-Host "  Lost packets: $lost" -ForegroundColor Yellow
        }
    }
}

Write-Host ""
Write-Host "=== Interactive Mode ===" -ForegroundColor Magenta
Write-Host "Type commands to send (or 'help' for commands, 'quit' to exit):" -ForegroundColor Cyan
Write-Host ""

while ($true) {
    Write-Host "Command> " -NoNewline -ForegroundColor Cyan
    $cmd = Read-Host
    
    if ($cmd -eq "quit" -or $cmd -eq "exit" -or $cmd -eq "q") {
        break
    }
    
    if ($cmd -eq "help" -or $cmd -eq "h") {
        Write-Host ""
        Write-Host "Available commands:" -ForegroundColor Yellow
        Write-Host "  PNG              - Ping (test RS-485)" -ForegroundColor White
        Write-Host "  NODE_TYPE        - Get node type" -ForegroundColor White
        Write-Host "  CFG ...          - Configure UWB (see examples below)" -ForegroundColor White
        Write-Host "  STRT             - Start test" -ForegroundColor White
        Write-Host "  STOP             - Stop test" -ForegroundColor White
        Write-Host "  STAT             - Get statistics" -ForegroundColor White
        Write-Host "  RST              - Reset statistics" -ForegroundColor White
        Write-Host ""
        Write-Host "Example CFG commands:" -ForegroundColor Yellow
        Write-Host "  TX: CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100" -ForegroundColor White
        Write-Host "  RX: CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100" -ForegroundColor White
        Write-Host ""
        continue
    }
    
    if ($cmd.Length -gt 0) {
        Send-Command $cmd
    }
}

Write-Host ""
Write-Host "Closing port..." -ForegroundColor Gray
$port.Close()
Write-Host "Done!" -ForegroundColor Green

