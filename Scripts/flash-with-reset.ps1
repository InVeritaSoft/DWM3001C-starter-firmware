# Flash with reset button held
# 1. Press and HOLD the reset button on the board
# 2. Run this script
# 3. When you see "NOW RELEASE RESET", release the button
# 4. Wait for flashing to complete

param([string]$BusId = "8-1")

$projectRoot = Split-Path -Parent $PSScriptRoot
$wslPath = "/mnt/c" + $projectRoot.Substring(2).Replace('\', '/')

Write-Host "=== FLASH WITH RESET BUTTON ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "STEP 1: Make sure you are HOLDING the reset button on the board!" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press ENTER when you are holding the reset button..." -ForegroundColor Yellow
Read-Host

Write-Host ""
Write-Host "Detaching USB devices..." -ForegroundColor Gray
usbipd detach --busid 5-1 2>$null
usbipd detach --busid 8-1 2>$null
Start-Sleep -Seconds 2

Write-Host "Attaching target device (Bus ID $BusId)..." -ForegroundColor Cyan
usbipd attach --wsl --busid $BusId
Start-Sleep -Seconds 3

Write-Host ""
Write-Host ">>> NOW RELEASE THE RESET BUTTON! <<<" -ForegroundColor Green
Write-Host ""
Start-Sleep -Seconds 1

Write-Host "Flashing firmware..." -ForegroundColor Cyan
$result = wsl bash -c "docker run --privileged -v /dev/bus/usb:/dev/bus/usb -v '$wslPath/Output:/project/Output:ro' uberi/qorvo-nrf52833-board nrfjprog --force -f nrf52 --program /project/Output/Common/Exe/dw3000_api.hex --sectorerase --verify --reset" 2>&1
Write-Host $result

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== FLASH SUCCESSFUL! ===" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "=== FLASH FAILED ===" -ForegroundColor Red
    Write-Host "Try again - make sure to hold reset before running, release after 'NOW RELEASE'" -ForegroundColor Yellow
}

# Detach so Windows can use the COM port
Write-Host ""
Write-Host "Detaching device for Windows access..." -ForegroundColor Gray
usbipd detach --busid $BusId 2>$null
