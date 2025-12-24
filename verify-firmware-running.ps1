# Verify firmware is running and check COM port configuration
# Usage: .\verify-firmware-running.ps1

Write-Host "=== Firmware Verification ===" -ForegroundColor Cyan
Write-Host ""

# Check if COM ports exist
Write-Host "Checking COM ports..." -ForegroundColor Yellow
$comPorts = @("COM17", "COM18")

foreach ($port in $comPorts) {
    $portExists = [System.IO.Ports.SerialPort]::GetPortNames() -contains $port
    if ($portExists) {
        Write-Host "  ${port}: EXISTS" -ForegroundColor Green
        
        # Try to get port info
        try {
            $portObj = New-Object System.IO.Ports.SerialPort $port
            Write-Host "    Description: $($portObj.PortName)" -ForegroundColor Gray
        } catch {
            Write-Host "    Could not query port details" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ${port}: NOT FOUND" -ForegroundColor Red
        Write-Host "    Check Device Manager for correct COM port" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Checking if firmware was rebuilt..." -ForegroundColor Yellow
$hexFile = "Output\Common\Exe\dw3000_api.hex"
if (Test-Path $hexFile) {
    $hexTime = (Get-Item $hexFile).LastWriteTime
    Write-Host "  Firmware file found: $hexFile" -ForegroundColor Green
    Write-Host "    Last modified: $hexTime" -ForegroundColor Gray
    
    $timeSinceBuild = (Get-Date) - $hexTime
    if ($timeSinceBuild.TotalMinutes -lt 10) {
        Write-Host "    Recently built (within last 10 minutes)" -ForegroundColor Green
    } else {
        Write-Host "    WARNING: Built more than 10 minutes ago" -ForegroundColor Yellow
        Write-Host "      Rebuild with fixes: .\build-and-flash-tx.ps1" -ForegroundColor White
    }
} else {
    Write-Host "  ERROR: Firmware file not found!" -ForegroundColor Red
    Write-Host "    Build firmware first: .\build.ps1" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Next Steps ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Verify boards are POWERED ON:" -ForegroundColor Yellow
Write-Host "   - Check power LED on boards" -ForegroundColor White
Write-Host "   - LEDs should blink once on startup" -ForegroundColor White
Write-Host ""
Write-Host "2. Rebuild firmware with fixes:" -ForegroundColor Yellow
Write-Host "   .\build-and-flash-tx.ps1" -ForegroundColor White
Write-Host "   .\build-and-flash-rx.ps1" -ForegroundColor White
Write-Host ""
Write-Host "3. After flashing, wait 5 seconds for initialization" -ForegroundColor Yellow
Write-Host ""
Write-Host "4. Test again:" -ForegroundColor Yellow
Write-Host "   .\test-uart-direct.ps1" -ForegroundColor White
Write-Host ""
Write-Host "5. If still no response, check RS-485 wiring:" -ForegroundColor Yellow
Write-Host "   - A+ to A+ (all devices)" -ForegroundColor White
Write-Host "   - B- to B- (all devices)" -ForegroundColor White
Write-Host "   - GND to GND (all devices)" -ForegroundColor White
Write-Host "   - Termination resistors (120Ω at each end)" -ForegroundColor White
