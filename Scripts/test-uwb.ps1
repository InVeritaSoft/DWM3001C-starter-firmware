# Test UWB communication between Node A (TX) and Node B (RX)
# Usage: .\test-uwb.ps1

param(
    [string]$TxPort = "COM15",
    [string]$RxPort = "COM11"
)

function Send-Command {
    param($port, $cmd)
    
    if ($port.BytesToRead -gt 0) { $null = $port.ReadExisting() }
    
    $port.Write("$cmd`r")
    Start-Sleep -Milliseconds 500
    
    $response = ""
    $maxWait = 20
    while ($maxWait -gt 0) {
        if ($port.BytesToRead -gt 0) {
            $response += $port.ReadExisting()
        }
        Start-Sleep -Milliseconds 50
        $maxWait--
        if ($response -match "`r`n|`n") { break }
    }
    return $response.Trim()
}

try {
    # Open TX port
    $tx = New-Object System.IO.Ports.SerialPort $TxPort, 115200, "None", 8, "One"
    $tx.ReadTimeout = 3000
    $tx.WriteTimeout = 2000
    $tx.DtrEnable = $false
    $tx.Open()
    
    # Open RX port
    $rx = New-Object System.IO.Ports.SerialPort $RxPort, 115200, "None", 8, "One"
    $rx.ReadTimeout = 3000
    $rx.WriteTimeout = 2000
    $rx.DtrEnable = $false
    $rx.Open()
    
    Write-Host "=== UWB Communication Test ===" -ForegroundColor Cyan
    Write-Host "TX Node: $TxPort" -ForegroundColor Yellow
    Write-Host "RX Node: $RxPort" -ForegroundColor Yellow
    Write-Host ""
    
    # Verify node types
    $txType = Send-Command $tx "NT"
    $rxType = Send-Command $rx "NT"
    Write-Host "TX Node Type: $txType" -ForegroundColor $(if ($txType -match "TX") { "Green" } else { "Red" })
    Write-Host "RX Node Type: $rxType" -ForegroundColor $(if ($rxType -match "RX") { "Green" } else { "Red" })
    Write-Host ""
    
    # Get initial stats
    $txStatBefore = Send-Command $tx "STAT"
    $rxStatBefore = Send-Command $rx "STAT"
    Write-Host "TX Stats Before: $txStatBefore" -ForegroundColor Gray
    Write-Host "RX Stats Before: $rxStatBefore" -ForegroundColor Gray
    Write-Host ""
    
    # Start both nodes (START now auto-configures if needed)
    Write-Host "Starting RX node..." -ForegroundColor Cyan
    $rxStart = Send-Command $rx "START"
    Write-Host "RX START: $rxStart" -ForegroundColor $(if ($rxStart -match "OK") { "Green" } else { "Red" })
    
    Start-Sleep -Milliseconds 1000
    
    Write-Host "Starting TX node..." -ForegroundColor Cyan
    $txStart = Send-Command $tx "START"
    Write-Host "TX START: $txStart" -ForegroundColor $(if ($txStart -match "OK") { "Green" } else { "Red" })
    Write-Host ""
    
    # Wait for some transmissions
    Write-Host "Waiting 5 seconds for UWB packets..." -ForegroundColor Yellow
    Start-Sleep -Seconds 5
    
    # Get stats after
    $txStatAfter = Send-Command $tx "STAT"
    $rxStatAfter = Send-Command $rx "STAT"
    Write-Host ""
    Write-Host "TX Stats After: $txStatAfter" -ForegroundColor Cyan
    Write-Host "RX Stats After: $rxStatAfter" -ForegroundColor Cyan
    Write-Host ""
    
    # Stop both nodes
    Write-Host "Stopping nodes..." -ForegroundColor Yellow
    $txStop = Send-Command $tx "STOP"
    $rxStop = Send-Command $rx "STOP"
    Write-Host "TX STOP: $txStop"
    Write-Host "RX STOP: $rxStop"
    
    # Parse and compare stats
    if ($txStatAfter -match "sent=(\d+)") {
        $txSent = [int]$matches[1]
        Write-Host ""
        if ($txSent -gt 0) {
            Write-Host "=== SUCCESS: TX sent $txSent packets ===" -ForegroundColor Green
        } else {
            Write-Host "=== WARNING: TX sent 0 packets ===" -ForegroundColor Yellow
        }
    }
    
    if ($rxStatAfter -match "received=(\d+)") {
        $rxReceived = [int]$matches[1]
        if ($rxReceived -gt 0) {
            Write-Host "=== SUCCESS: RX received $rxReceived packets ===" -ForegroundColor Green
        } else {
            Write-Host "=== WARNING: RX received 0 packets ===" -ForegroundColor Yellow
        }
    }
    
    $tx.Close()
    $rx.Close()
    
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    if ($tx -and $tx.IsOpen) { $tx.Close() }
    if ($rx -and $rx.IsOpen) { $rx.Close() }
    exit 1
}
