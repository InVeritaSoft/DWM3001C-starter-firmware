# Simple flash script for DWM3001C firmware
# Usage: .\simple-flash.ps1 -BusId 8-1

param([string]$BusId = "")

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = $projectRoot.Replace('\', '/')

$hexFile = Join-Path $projectRoot "Output\Common\Exe\dw3000_api.hex"
if (-not (Test-Path $hexFile)) {
    Write-Host "Error: Firmware not found at $hexFile" -ForegroundColor Red
    exit 1
}

Write-Host "Flashing DWM3001C firmware..." -ForegroundColor Green

# List devices
$usbipdExe = "usbipd"
$usbDevices = & $usbipdExe list 2>&1
Write-Host "Available devices:" -ForegroundColor Cyan
$usbDevices | Select-String -Pattern "SEGGER|J-Link|1366" | ForEach-Object { Write-Host "  $_" }

if (-not $BusId) {
    Write-Host ""
    Write-Host "Please specify -BusId parameter (e.g. 8-1 or 5-1)" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Target Bus ID: $BusId" -ForegroundColor Yellow

# Detach all J-Link devices first
Write-Host "Detaching all J-Link devices..." -ForegroundColor Cyan
& $usbipdExe detach --busid "5-1" 2>&1 | Out-Null
& $usbipdExe detach --busid "8-1" 2>&1 | Out-Null
Start-Sleep -Seconds 2

# Attach only target device
Write-Host "Attaching target device (Bus ID $BusId)..." -ForegroundColor Cyan
& $usbipdExe attach --wsl --busid $BusId 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to attach device!" -ForegroundColor Red
    exit 1
}

Start-Sleep -Seconds 4

# Convert Windows path to WSL2 path
$wslPath = $projectPath.Replace('C:', '/mnt/c').Replace('\', '/')

# Flash using WSL
Write-Host "Flashing via WSL2..." -ForegroundColor Green
wsl bash -c "cd '$wslPath' && docker run --privileged -v /dev/bus/usb:/dev/bus/usb -v '$wslPath/Output:/project/Output:ro' uberi/qorvo-nrf52833-board nrfjprog --force -f nrf52 --program /project/Output/Common/Exe/dw3000_api.hex --sectorerase --verify --reset"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Flash completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Flash failed!" -ForegroundColor Red
    exit 1
}
