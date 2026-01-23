# Comprehensive Test Script for Both UWB Nodes
# Tests all available commands on Node A (TX) and Node B (RX)
#
# Usage:
#   .\test-all-commands.ps1
#   .\test-all-commands.ps1 -NodeAPort COM20 -NodeBPort COM21
#   .\test-all-commands.ps1 -Baudrate 57600

param(
    [string]$NodeAPort = "COM20",
    [string]$NodeBPort = "COM21",
    [int]$Baudrate = 57600
)

# Try to load from .env.win
$envFile = Join-Path $PSScriptRoot "..\Orchestrator\.env.win"
if (Test-Path $envFile) {
    Get-Content $envFile | ForEach-Object {
        if ($_ -match '^([^#=]+)=(.*)$') {
            $key = $matches[1].Trim()
            $value = $matches[2].Trim()
            switch ($key) {
                "RS485_NODE_A_PORT" { if (-not $PSBoundParameters.ContainsKey('NodeAPort')) { $NodeAPort = $value } }
                "RS485_NODE_B_PORT" { if (-not $PSBoundParameters.ContainsKey('NodeBPort')) { $NodeBPort = $value } }
                "BAUDRATE" { if (-not $PSBoundParameters.ContainsKey('Baudrate')) { $Baudrate = [int]$value } }
            }
        }
    }
}

$ErrorActionPreference = "Continue"
$results = @()

function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host ("=" * 70) -ForegroundColor Cyan
    Write-Host $Text.PadLeft(($Text.Length + 70) / 2) -ForegroundColor Cyan
    Write-Host ("=" * 70) -ForegroundColor Cyan
    Write-Host ""
}

function Write-Test {
    param([string]$Text)
    Write-Host "[TEST] " -NoNewline -ForegroundColor Cyan
    Write-Host $Text
}

function Write-Success {
    param([string]$Text)
    Write-Host "[✓] " -NoNewline -ForegroundColor Green
    Write-Host $Text
}

function Write-Error {
    param([string]$Text)
    Write-Host "[✗] " -NoNewline -ForegroundColor Red
    Write-Host $Text
}

function Write-Info {
    param([string]$Text)
    Write-Host "[INFO] " -NoNewline -ForegroundColor Blue
    Write-Host $Text
}

function Send-Command {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$TimeoutMs = 5000
    )
    
    if ($Port.BytesToRead -gt 0) {
        $null = $Port.ReadExisting()
    }
    
    $Port.Write("$Command`r`n")
    Start-Sleep -Milliseconds 100
    
    $response = ""
    $startTime = Get-Date
    $timeout = New-TimeSpan -Milliseconds $TimeoutMs
    
    while ((Get-Date) - $startTime -lt $timeout) {
        if ($Port.BytesToRead -gt 0) {
            $response += $Port.ReadExisting()
            if ($response -match "`r`n|`n") {
                break
            }
        }
        Start-Sleep -Milliseconds 50
    }
    
    return $response.Trim()
}

function Test-Command {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$NodeId,
        [string]$TestName,
        [string]$Command,
        [int]$TimeoutMs = 5000
    )
    
    Write-Test "$NodeId`: $TestName"
    
    $startTime = Get-Date
    $success = $false
    $response = ""
    $error = ""
    
    try {
        $response = Send-Command -Port $Port -Command $Command -TimeoutMs $TimeoutMs
        
        if ($response -match "^OK") {
            $success = $true
            $duration = ((Get-Date) - $startTime).TotalMilliseconds
            Write-Success "$NodeId`: $TestName - $($response.Substring(0, [Math]::Min(80, $response.Length)))"
            if ($duration -gt 1000) {
                Write-Info "Duration: $([Math]::Round($duration, 1))ms"
            }
        } else {
            $error = "Unexpected response: $response"
            Write-Error "$NodeId`: $TestName - $error"
        }
    } catch {
        $error = $_.Exception.Message
        Write-Error "$NodeId`: $TestName - $error"
    }
    
    $duration = ((Get-Date) - $startTime).TotalMilliseconds
    $results += [PSCustomObject]@{
        NodeId = $NodeId
        TestName = $TestName
        Success = $success
        Response = $response
        Error = $error
        DurationMs = $duration
    }
    
    Start-Sleep -Milliseconds 200
}

# Main execution
Write-Host ""
Write-Host ("=" * 70) -ForegroundColor Cyan
Write-Host ("UWB Node Command Test Suite".PadLeft(("UWB Node Command Test Suite".Length + 70) / 2)) -ForegroundColor Cyan
Write-Host ("=" * 70) -ForegroundColor Cyan
Write-Host ""
Write-Host "Node A (TX) Port: $NodeAPort"
Write-Host "Node B (RX) Port: $NodeBPort"
Write-Host "Baud Rate: $Baudrate"
Write-Host "Start Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host ""

# Open serial ports
Write-Header "Setting Up Connections"

try {
    Write-Info "Opening Node A port..."
    $portA = New-Object System.IO.Ports.SerialPort $NodeAPort, $Baudrate, "None", 8, "One"
    $portA.ReadTimeout = 5000
    $portA.WriteTimeout = 2000
    $portA.DtrEnable = $false
    $portA.Open()
    Write-Success "Node A connected"
    
    Write-Info "Opening Node B port..."
    $portB = New-Object System.IO.Ports.SerialPort $NodeBPort, $Baudrate, "None", 8, "One"
    $portB.ReadTimeout = 5000
    $portB.WriteTimeout = 2000
    $portB.DtrEnable = $false
    $portB.Open()
    Write-Success "Node B connected"
    
    Start-Sleep -Seconds 2  # Wait for nodes to initialize
    
    # Test 1: PING
    Write-Header "Test 1: PING (Connectivity Check)"
    Test-Command -Port $portA -NodeId "A" -TestName "PING" -Command "PNG"
    Test-Command -Port $portB -NodeId "B" -TestName "PING" -Command "PNG"
    
    # Test 2: NODE_TYPE
    Write-Header "Test 2: NODE_TYPE (Get Node Type)"
    Test-Command -Port $portA -NodeId "A" -TestName "NODE_TYPE" -Command "NODE_TYPE"
    Test-Command -Port $portB -NodeId "B" -TestName "NODE_TYPE" -Command "NODE_TYPE"
    
    # Test 3: SET_CONFIG
    Write-Header "Test 3: SET_CONFIG (Configure UWB Parameters)"
    $configCmd = "CFG ch=5 rate=6m8 pl=128 len=64 pwr=5 rate_hz=100"
    Write-Info "Configuring with: $configCmd"
    Test-Command -Port $portA -NodeId "A" -TestName "SET_CONFIG" -Command $configCmd -TimeoutMs 10000
    Test-Command -Port $portB -NodeId "B" -TestName "SET_CONFIG" -Command $configCmd -TimeoutMs 10000
    
    # Test 4: GET_STATS (before)
    Write-Header "Test 4: GET_STATS (Before Test)"
    Test-Command -Port $portA -NodeId "A" -TestName "GET_STATS" -Command "STAT"
    Test-Command -Port $portB -NodeId "B" -TestName "GET_STATS" -Command "STAT"
    
    # Test 5: RESET_STATS
    Write-Header "Test 5: RESET_STATS (Reset Statistics)"
    Test-Command -Port $portA -NodeId "A" -TestName "RESET_STATS" -Command "RST"
    Test-Command -Port $portB -NodeId "B" -TestName "RESET_STATS" -Command "RST"
    
    # Test 6: START_TEST
    Write-Header "Test 6: START_TEST (Start UWB Test)"
    Test-Command -Port $portA -NodeId "A" -TestName "START_TEST" -Command "STRT"
    Test-Command -Port $portB -NodeId "B" -TestName "START_TEST" -Command "STRT"
    Start-Sleep -Seconds 2  # Let test run
    
    # Test 7: GET_STATS (during)
    Write-Header "Test 7: GET_STATS (During Test)"
    Test-Command -Port $portA -NodeId "A" -TestName "GET_STATS (running)" -Command "STAT"
    Test-Command -Port $portB -NodeId "B" -TestName "GET_STATS (running)" -Command "STAT"
    
    # Test 8: STOP_TEST
    Write-Header "Test 8: STOP_TEST (Stop UWB Test)"
    Test-Command -Port $portA -NodeId "A" -TestName "STOP_TEST" -Command "STOP"
    Test-Command -Port $portB -NodeId "B" -TestName "STOP_TEST" -Command "STOP"
    Start-Sleep -Seconds 1
    
    # Test 9: GET_STATS (after)
    Write-Header "Test 9: GET_STATS (After Test)"
    Test-Command -Port $portA -NodeId "A" -TestName "GET_STATS (after)" -Command "STAT"
    Test-Command -Port $portB -NodeId "B" -TestName "GET_STATS (after)" -Command "STAT"
    
    # Test 10: SET_LOG_MODE
    Write-Header "Test 10: SET_LOG_MODE (Set Log Level)"
    Test-Command -Port $portA -NodeId "A" -TestName "SET_LOG_MODE" -Command "SET_LOG_MODE level=1"
    Test-Command -Port $portB -NodeId "B" -TestName "SET_LOG_MODE" -Command "SET_LOG_MODE level=1"
    
    # Test 11: Multiple configurations
    Write-Header "Test 11: Multiple Configuration Scenarios"
    $configs = @(
        @{Name="Channel 9"; Cmd="CFG ch=9 rate=6m8 pl=128 len=64 pwr=5 rate_hz=50"},
        @{Name="850k rate"; Cmd="CFG ch=5 rate=850k pl=256 len=32 pwr=3 rate_hz=10"},
        @{Name="Long preamble"; Cmd="CFG ch=5 rate=6m8 pl=512 len=128 pwr=7 rate_hz=200"}
    )
    
    foreach ($cfg in $configs) {
        Write-Info "Testing config: $($cfg.Name)"
        Test-Command -Port $portA -NodeId "A" -TestName "SET_CONFIG ($($cfg.Name))" -Command $cfg.Cmd -TimeoutMs 10000
        Test-Command -Port $portB -NodeId "B" -TestName "SET_CONFIG ($($cfg.Name))" -Command $cfg.Cmd -TimeoutMs 10000
    }
    
    # Restore default
    $defaultCmd = "CFG ch=5 rate=6m8 pl=128 len=64 pwr=5 rate_hz=100"
    Test-Command -Port $portA -NodeId "A" -TestName "SET_CONFIG (restore default)" -Command $defaultCmd -TimeoutMs 10000
    Test-Command -Port $portB -NodeId "B" -TestName "SET_CONFIG (restore default)" -Command $defaultCmd -TimeoutMs 10000
    
    # Print summary
    Write-Header "Test Summary"
    
    $total = $results.Count
    $passed = ($results | Where-Object { $_.Success }).Count
    $failed = $total - $passed
    
    Write-Host "Total Tests: $total"
    Write-Host "Passed: $passed" -ForegroundColor Green
    Write-Host "Failed: $failed" -ForegroundColor Red
    Write-Host "Success Rate: $([Math]::Round($passed/$total*100, 1))%"
    
    if ($failed -gt 0) {
        Write-Host ""
        Write-Host "Failed Tests:" -ForegroundColor Red
        foreach ($result in $results | Where-Object { -not $_.Success }) {
            Write-Host "  [✗] $($result.NodeId): $($result.TestName)" -ForegroundColor Red
            Write-Host "    Error: $($result.Error)"
        }
    }
    
    $nodeAResults = $results | Where-Object { $_.NodeId -eq "A" }
    $nodeBResults = $results | Where-Object { $_.NodeId -eq "B" }
    
    Write-Host ""
    Write-Host "Node A Results:" -ForegroundColor Cyan
    $nodeAPassed = ($nodeAResults | Where-Object { $_.Success }).Count
    Write-Host "  Passed: $nodeAPassed/$($nodeAResults.Count)"
    
    Write-Host ""
    Write-Host "Node B Results:" -ForegroundColor Cyan
    $nodeBPassed = ($nodeBResults | Where-Object { $_.Success }).Count
    Write-Host "  Passed: $nodeBPassed/$($nodeBResults.Count)"
    
    $avgTime = ($results | Measure-Object -Property DurationMs -Average).Average
    Write-Host ""
    Write-Host "Average Response Time: $([Math]::Round($avgTime, 1))ms" -ForegroundColor Cyan
    
} catch {
    Write-Error "Fatal error: $_"
    Write-Host $_.ScriptStackTrace
} finally {
    # Cleanup
    Write-Header "Cleaning Up"
    
    if ($portA -and $portA.IsOpen) {
        try {
            Send-Command -Port $portA -Command "STOP" -TimeoutMs 2000 | Out-Null
        } catch {}
        try {
            $portA.Close()
            Write-Success "Node A disconnected"
        } catch {
            Write-Error "Error disconnecting Node A: $_"
        }
    }
    
    if ($portB -and $portB.IsOpen) {
        try {
            Send-Command -Port $portB -Command "STOP" -TimeoutMs 2000 | Out-Null
        } catch {}
        try {
            $portB.Close()
            Write-Success "Node B disconnected"
        } catch {
            Write-Error "Error disconnecting Node B: $_"
        }
    }
}

Write-Host ""
Write-Host "Test complete!" -ForegroundColor Green
