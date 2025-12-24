# Automated Build, Flash, and Test Script with Iteration Support
# Usage: .\auto-test-tx-rx.ps1 [iterations]
# Example: .\auto-test-tx-rx.ps1 3  (runs 3 iterations)

param(
    [int]$Iterations = 1,
    [int]$TestDurationSeconds = 30,
    [switch]$SkipBuild = $false,
    [switch]$SkipFlash = $false
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$logFile = Join-Path $projectRoot ".cursor\debug.log"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  UWB TX/RX Automated Test Script" -ForegroundColor Cyan
Write-Host "  Iterations: $Iterations" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Serial Ports Configuration:" -ForegroundColor Yellow
Write-Host "  Node A (TX): COM20" -ForegroundColor White
Write-Host "  Node B (RX): COM22" -ForegroundColor White
Write-Host ""
Write-Host "Make sure both boards are connected via RS-485 to these COM ports" -ForegroundColor Cyan
Write-Host ""

# Function to clear debug log
function Clear-DebugLog {
    if (Test-Path $logFile) {
        Remove-Item $logFile -Force
        Write-Host "[LOG] Cleared debug log" -ForegroundColor Gray
    }
}

# Function to build firmware
function Build-Firmware {
    param([string]$NodeType)
    
    Write-Host "[BUILD] Building $NodeType firmware..." -ForegroundColor Yellow
    
    if ($NodeType -eq "TX") {
        & "$projectRoot\set-node-tx.ps1"
    } else {
        & "$projectRoot\set-node-rx.ps1"
    }
    
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure for $NodeType"
    }
    
    & "$projectRoot\build.ps1"
    
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for $NodeType"
    }
    
    Write-Host "[BUILD] $NodeType build successful" -ForegroundColor Green
}

# Function to flash firmware
function Flash-Firmware {
    param([string]$NodeType)
    
    Write-Host "[FLASH] Flashing $NodeType node..." -ForegroundColor Yellow
    Write-Host "       Please ensure $NodeType board is connected and powered on" -ForegroundColor Gray
    
    # Check if USB setup is needed (one-time check)
    $usbipdCheck = Get-Command usbipd -ErrorAction SilentlyContinue
    if ($usbipdCheck) {
        $sharedCheck = wsl bash -c "ls -la /dev/bus/usb 2>&1" 2>&1
        if ($LASTEXITCODE -ne 0 -or $sharedCheck -match "No such file") {
            Write-Host "[SETUP] USB device not shared with WSL2. Running setup..." -ForegroundColor Yellow
            Write-Host "        (This may require admin ONCE for installation)" -ForegroundColor Gray
            & "$projectRoot\setup-usb.ps1"
            if ($LASTEXITCODE -ne 0) {
                Write-Host "[SETUP] USB setup failed. Please run manually:" -ForegroundColor Red
                Write-Host "        .\setup-usb.ps1" -ForegroundColor White
                throw "USB setup required"
            }
        }
    }
    
    & "$projectRoot\flash.ps1"
    
    if ($LASTEXITCODE -ne 0) {
        throw "Flash failed for $NodeType"
    }
    
    Write-Host "[FLASH] $NodeType flash successful" -ForegroundColor Green
}

# Function to run test
function Run-Test {
    param([int]$IterationNumber)
    
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  Test Iteration $IterationNumber of $Iterations" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    # Clear debug log before each test
    Clear-DebugLog
    
    Write-Host "[TEST] Starting Orchestrator test..." -ForegroundColor Yellow
    Write-Host "       Test duration: $TestDurationSeconds seconds" -ForegroundColor Gray
    Write-Host ""
    
    # Change to Orchestrator directory
    Push-Location "$projectRoot\Orchestrator"
    
    try {
        # Run Orchestrator (it will run the test plan)
        $process = Start-Process -FilePath "node" -ArgumentList "src/orchestrator/main.js" -NoNewWindow -PassThru -Wait
        
        if ($process.ExitCode -ne 0) {
            Write-Host "[TEST] Orchestrator exited with code $($process.ExitCode)" -ForegroundColor Yellow
        }
    } finally {
        Pop-Location
    }
    
    # Check if log file was created
    if (Test-Path $logFile) {
        $logSize = (Get-Item $logFile).Length
        Write-Host "[LOG] Debug log created: $logSize bytes" -ForegroundColor Green
    } else {
        Write-Host "[LOG] Warning: Debug log not found" -ForegroundColor Yellow
    }
    
    Write-Host "[TEST] Iteration $IterationNumber complete" -ForegroundColor Green
    Write-Host ""
}

# Main execution
try {
    # Step 1: Build and Flash TX Node (unless skipped)
    if (-not $SkipBuild) {
        Write-Host "[SETUP] Step 1/4: Building TX Node..." -ForegroundColor Cyan
        Build-Firmware "TX"
        Write-Host ""
    }
    
    if (-not $SkipFlash) {
        Write-Host "[SETUP] Step 2/4: Flashing TX Node..." -ForegroundColor Cyan
        Write-Host "        Connect TX board (Node A) now and press any key to continue..." -ForegroundColor Yellow
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        Flash-Firmware "TX"
        Write-Host ""
    }
    
    # Step 2: Build and Flash RX Node (unless skipped)
    if (-not $SkipBuild) {
        Write-Host "[SETUP] Step 3/4: Building RX Node..." -ForegroundColor Cyan
        Build-Firmware "RX"
        Write-Host ""
    }
    
    if (-not $SkipFlash) {
        Write-Host "[SETUP] Step 4/4: Flashing RX Node..." -ForegroundColor Cyan
        Write-Host "        Connect RX board (Node B) now and press any key to continue..." -ForegroundColor Yellow
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        Flash-Firmware "RX"
        Write-Host ""
    }
    
    # Wait for boards to initialize
    Write-Host "[SETUP] Waiting for boards to initialize (5 seconds)..." -ForegroundColor Yellow
    Start-Sleep -Seconds 5
    Write-Host ""
    
    # Step 3: Run tests iteratively
    for ($i = 1; $i -le $Iterations; $i++) {
        Run-Test -IterationNumber $i
        
        # Wait between iterations (except last one)
        if ($i -lt $Iterations) {
            Write-Host "Waiting 3 seconds before next iteration..." -ForegroundColor Gray
            Start-Sleep -Seconds 3
        }
    }
    
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  All Tests Complete!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Debug logs saved to: $logFile" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "To analyze logs, check:" -ForegroundColor Yellow
    Write-Host "  .cursor\debug.log" -ForegroundColor White
    
} catch {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  ERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Stack trace:" -ForegroundColor Yellow
    Write-Host $_.ScriptStackTrace -ForegroundColor Gray
    exit 1
}
