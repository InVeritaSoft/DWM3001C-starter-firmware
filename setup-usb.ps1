# Setup USB device sharing with WSL2 (one-time setup)
# This script finds J-Link devices and shares them with WSL2
# Usage: .\setup-usb.ps1
# Note: This may require admin privileges ONCE to install usbipd-win

Write-Host "=== USB Device Setup for WSL2 ===" -ForegroundColor Cyan
Write-Host ""

# Check if WSL2 is available
$wslCheck = wsl --status 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "WSL2 not found or not configured!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install WSL2:" -ForegroundColor Yellow
    Write-Host "  wsl --install" -ForegroundColor White
    Write-Host ""
    Write-Host "Or enable it via:" -ForegroundColor Yellow
    Write-Host "  dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart" -ForegroundColor White
    Write-Host "  dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart" -ForegroundColor White
    Write-Host ""
    Write-Host "Then restart your computer and try again." -ForegroundColor Yellow
    exit 1
}

Write-Host "WSL2 is available ✓" -ForegroundColor Green
Write-Host ""

# Find usbipd executable
$usbipdExe = $null

# Try to find usbipd in common installation locations
$usbipdPaths = @(
    "$env:ProgramFiles\usbipd-win\usbipd.exe",
    "${env:ProgramFiles(x86)}\usbipd-win\usbipd.exe",
    "$env:LOCALAPPDATA\Microsoft\WindowsApps\usbipd.exe"
)

# Check if usbipd is in PATH
$usbipdCheck = Get-Command usbipd -ErrorAction SilentlyContinue
if ($usbipdCheck) {
    $usbipdExe = $usbipdCheck.Source
    Write-Host "Found usbipd at: $usbipdExe" -ForegroundColor Green
} else {
    # Try to find in common locations
    foreach ($path in $usbipdPaths) {
        if (Test-Path $path) {
            $usbipdExe = $path
            Write-Host "Found usbipd at: $usbipdExe" -ForegroundColor Green
            break
        }
    }
}

# If still not found, try to install
if (-not $usbipdExe) {
    Write-Host "usbipd-win not found. Installing..." -ForegroundColor Yellow
    Write-Host "This may require admin privileges..." -ForegroundColor Yellow
    Write-Host ""
    
    # Try to install via winget
    $installOutput = winget install --id usbipd-win.usbipd-win -e --accept-source-agreements --accept-package-agreements 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "Failed to install usbipd-win automatically." -ForegroundColor Red
        Write-Host ""
        Write-Host "Please install manually (may require admin):" -ForegroundColor Yellow
        Write-Host "  winget install usbipd-win" -ForegroundColor White
        Write-Host ""
        Write-Host "Or download from: https://github.com/dorssel/usbipd-win/releases" -ForegroundColor White
        Write-Host ""
        Write-Host "After installation, restart PowerShell and run this script again." -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "usbipd-win installed successfully!" -ForegroundColor Green
    Write-Host ""
    
    # Refresh PATH
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
    
    # Try to find again after installation
    foreach ($path in $usbipdPaths) {
        if (Test-Path $path) {
            $usbipdExe = $path
            Write-Host "Found usbipd at: $usbipdExe" -ForegroundColor Green
            break
        }
    }
    
    if (-not $usbipdExe) {
        Write-Host ""
        Write-Host "usbipd installed but not found." -ForegroundColor Yellow
        Write-Host "Please restart PowerShell and run this script again." -ForegroundColor Yellow
        exit 1
    }
}

Write-Host ""

# List USB devices (new syntax: usbipd list, not usbipd wsl list)
Write-Host "Scanning for USB devices..." -ForegroundColor Yellow
$usbDevices = & $usbipdExe list 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Error running usbipd." -ForegroundColor Red
    Write-Host "Command: $usbipdExe list" -ForegroundColor Gray
    Write-Host "Error output: $usbDevices" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Make sure usbipd-win is installed: winget install usbipd-win" -ForegroundColor White
    Write-Host "  2. Restart PowerShell after installation" -ForegroundColor White
    Write-Host "  3. Check if usbipd is in PATH: Get-Command usbipd" -ForegroundColor White
    Write-Host ""
    Write-Host "If usbipd is installed but not found, restart PowerShell and try again." -ForegroundColor Yellow
    exit 1
}

Write-Host $usbDevices
Write-Host ""

# Find J-Link devices (SEGGER devices)
$jlinkDevices = $usbDevices | Select-String -Pattern "SEGGER|J-Link|1366" -CaseSensitive:$false

if ($jlinkDevices.Count -eq 0) {
    Write-Host "No J-Link devices found!" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Make sure:" -ForegroundColor Cyan
    Write-Host "  1. DWM3001CDK board is connected via USB (J9 port)" -ForegroundColor White
    Write-Host "  2. Board is powered on" -ForegroundColor White
    Write-Host "  3. USB cable is properly connected" -ForegroundColor White
    Write-Host ""
    Write-Host "Run this script again after connecting the board." -ForegroundColor Yellow
    exit 1
}

Write-Host "Found J-Link device(s):" -ForegroundColor Green
$jlinkDevices | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
Write-Host ""

# Extract bus IDs and check if already attached/shared
$busIds = @()
$attachedBusIds = @()
foreach ($line in $usbDevices) {
    if ($line -match "SEGGER|J-Link|1366") {
        # Extract bus ID (format: BUSID  VID:PID    DESCRIPTION  STATE)
        # Skip GUID entries (Persisted section)
        if ($line -match "^\s*([0-9-]+)") {
            $busId = $matches[1]
            # Check if already attached or shared
            if ($line -match "Attached|Shared") {
                $attachedBusIds += $busId
                $state = if ($line -match "Attached") { "attached" } else { "shared" }
                Write-Host "  Bus ID $busId is already $state ✓" -ForegroundColor Green
            } else {
                $busIds += $busId
            }
        }
    }
}

if ($busIds.Count -eq 0 -and $attachedBusIds.Count -eq 0) {
    Write-Host "Could not parse bus IDs. Please attach devices manually:" -ForegroundColor Yellow
    Write-Host "  usbipd attach --wsl --busid <busid>" -ForegroundColor White
    exit 1
}

# If all devices are already attached/shared, we're done
if ($busIds.Count -eq 0 -and $attachedBusIds.Count -gt 0) {
    Write-Host ""
    Write-Host "All J-Link devices are already attached to WSL2 ✓" -ForegroundColor Green
} else {
    # Share each J-Link device with WSL2 (new syntax: attach --wsl --busid)
    Write-Host "Sharing J-Link device(s) with WSL2..." -ForegroundColor Yellow
    foreach ($busId in $busIds) {
        Write-Host "  Attaching bus ID: $busId to WSL2..." -ForegroundColor Cyan
        
        # New syntax: usbipd attach --wsl --busid <BUSID>
        & $usbipdExe attach --wsl --busid $busId 2>&1 | Out-Null
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✓ Successfully attached $busId to WSL2" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Failed to attach $busId" -ForegroundColor Red
            Write-Host ""
            Write-Host "    Troubleshooting:" -ForegroundColor Yellow
            Write-Host "    1. Make sure WSL2 is running: wsl --status" -ForegroundColor White
            Write-Host "    2. Try binding first: usbipd bind --busid $busId" -ForegroundColor White
            Write-Host "    3. Check device status: usbipd list" -ForegroundColor White
            Write-Host ""
            Write-Host "    If device shows 'Shared', it's already attached - you're good to go!" -ForegroundColor Cyan
        }
    }
}

Write-Host ""
Write-Host "=== Verifying Setup ===" -ForegroundColor Cyan

# Verify device is accessible in WSL2
$wslUsbCheck = wsl bash -c "ls -la /dev/bus/usb 2>&1" 2>&1
if ($LASTEXITCODE -eq 0 -and $wslUsbCheck -notmatch "No such file") {
    Write-Host "✓ USB devices are accessible in WSL2" -ForegroundColor Green
} else {
    Write-Host "⚠ USB devices may not be accessible in WSL2 yet" -ForegroundColor Yellow
    Write-Host "  Try restarting WSL2: wsl --shutdown" -ForegroundColor White
    Write-Host "  Then run this script again" -ForegroundColor White
}

Write-Host ""
Write-Host "=== Setup Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "USB devices are now shared with WSL2." -ForegroundColor Cyan
Write-Host "You can now flash firmware without admin privileges!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  .\build-and-flash-tx.ps1  (flash TX node)" -ForegroundColor White
Write-Host "  .\build-and-flash-rx.ps1  (flash RX node)" -ForegroundColor White
Write-Host ""
Write-Host "Note: If you unplug/replug USB, run this script again." -ForegroundColor Yellow
