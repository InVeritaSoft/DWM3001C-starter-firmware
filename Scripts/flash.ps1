# Flash script for DWM3001C firmware
# Usage: .\flash.ps1 [-BusId <busid>] [-ComPort <comport>] [-NodeType TX|RX]
# Examples:
#   .\flash.ps1 -BusId 4-1
#   .\flash.ps1 -ComPort COM11
#   .\flash.ps1 -NodeType TX
# Note: Requires Docker Desktop with WSL2 backend for USB device access

param(
    [string]$BusId = "",
    [string]$ComPort = "",
    [string]$NodeType = ""  # TX or RX
)

Write-Host "Flashing DWM3001C firmware..." -ForegroundColor Green

# Get the project root directory
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectPath = $projectRoot.Replace('\', '/')

# Check if hex file exists
$hexFile = Join-Path $projectRoot "Output\Common\Exe\dw3000_api.hex"
if (-not (Test-Path $hexFile)) {
    Write-Host "Error: Firmware not found at $hexFile" -ForegroundColor Red
    Write-Host "Please run .\build.ps1 first to build the firmware." -ForegroundColor Yellow
    exit 1
}

# Ensure Docker is running
$dockerRunning = docker info 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Docker is not running. Please start Docker Desktop." -ForegroundColor Red
    exit 1
}

Write-Host "Connecting to board via J-Link..." -ForegroundColor Yellow
Write-Host "Make sure the board is connected via USB (J9 port) and powered on." -ForegroundColor Yellow

# Check if running in WSL2
$isWSL = $env:WSL_DISTRO_NAME -ne $null

if ($isWSL) {
    # Running in WSL2 - can mount USB devices directly
    Write-Host "Detected WSL2 environment - using USB device mounting..." -ForegroundColor Cyan
    
    # Check if USB devices are accessible
    $usbCheck = wsl bash -c "ls -la /dev/bus/usb 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "WARNING: USB devices not accessible in WSL2." -ForegroundColor Yellow
        Write-Host "You may need to install usbipd-win and share the USB device:" -ForegroundColor Yellow
        Write-Host "  1. Install: winget install usbipd-win" -ForegroundColor Gray
        Write-Host "  2. List devices: usbipd list" -ForegroundColor Gray
        Write-Host "  3. Attach device: usbipd attach --wsl --busid <busid>" -ForegroundColor Gray
    }
    
    docker run --privileged `
        -v /dev/bus/usb:/dev/bus/usb `
        -v "${projectPath}/Output:/project/Output:ro" `
        uberi/qorvo-nrf52833-board `
        nrfjprog --force -f nrf52 --program /project/Output/Common/Exe/dw3000_api.hex --sectorerase --verify
} else {
    # Running in Windows PowerShell
    # Docker Desktop on Windows doesn't support USB passthrough
    Write-Host "Windows PowerShell detected." -ForegroundColor Cyan
    Write-Host ""
    
    # Check if usbipd is available
    $usbipdCheck = Get-Command usbipd -ErrorAction SilentlyContinue
    if (-not $usbipdCheck) {
        Write-Host "usbipd-win not found. Setting up USB sharing..." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Running setup script (may require admin ONCE for installation)..." -ForegroundColor Cyan
        & "$projectRoot\setup-usb.ps1"
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host ""
            Write-Host "USB setup failed. Please run manually:" -ForegroundColor Red
            Write-Host "  .\setup-usb.ps1" -ForegroundColor White
            Write-Host ""
            Write-Host "Or install usbipd-win and attach device manually:" -ForegroundColor Yellow
            Write-Host "  winget install usbipd-win" -ForegroundColor White
            Write-Host "  usbipd list" -ForegroundColor White
            Write-Host "  usbipd attach --wsl --busid <busid>" -ForegroundColor White
            exit 1
        }
    }
    
    # Find usbipd executable
    $usbipdExe = Get-Command usbipd -ErrorAction SilentlyContinue
    if (-not $usbipdExe) {
        # Try common location
        $usbipdExe = "$env:ProgramFiles\usbipd-win\usbipd.exe"
        if (-not (Test-Path $usbipdExe)) {
            Write-Host "usbipd not found. Please run: .\setup-usb.ps1" -ForegroundColor Red
            exit 1
        }
    } else {
        $usbipdExe = $usbipdExe.Source
    }
    
    # List all J-Link devices
    $usbDevices = & $usbipdExe list 2>&1
    $jlinkDevices = $usbDevices | Select-String -Pattern "SEGGER|J-Link|1366" -CaseSensitive:$false
    
    if ($jlinkDevices.Count -eq 0) {
        Write-Host ""
        Write-Host "No J-Link devices found!" -ForegroundColor Red
        Write-Host "Make sure board is connected and run:" -ForegroundColor Yellow
        Write-Host "  .\setup-usb.ps1" -ForegroundColor White
        exit 1
    }
    
    # Find the target device
    $targetBusId = $null
    $jlinkLine = $null
    
    # Helper function to show device selection menu
    function Show-DeviceMenu {
        param([array]$Devices, [string]$Prompt)
        
        Write-Host ""
        Write-Host $Prompt -ForegroundColor Yellow
        Write-Host ""
        $deviceList = @()
        $index = 1
        foreach ($device in $Devices) {
            # Get the line text from Select-String object
            $lineText = if ($device.Line) { $device.Line } else { $device.ToString() }
            
            # Try multiple regex patterns to extract Bus ID and COM port
            $devBusId = $null
            $devComPort = $null
            
            # Pattern 1: Bus ID and COM port on same line
            if ($lineText -match "^\s*([0-9-]+).*?(COM\d+)") {
                $devBusId = $matches[1]
                $devComPort = $matches[2]
            }
            # Pattern 2: Just Bus ID (COM port might be on another line or missing)
            elseif ($lineText -match "^\s*([0-9-]+)") {
                $devBusId = $matches[1]
                # Try to find COM port in the same line
                if ($lineText -match "(COM\d+)") {
                    $devComPort = $matches[1]
                } else {
                    $devComPort = "N/A"
                }
            }
            
            if ($devBusId) {
                $state = if ($lineText -match "Attached|Shared") { "✓ Attached" } else { "Not attached" }
                Write-Host "  [$index] Bus ID: $devBusId  COM Port: $devComPort  State: $state" -ForegroundColor Cyan
                # Create PSCustomObject for easier property access
                $deviceList += [PSCustomObject]@{
                    BusId = $devBusId
                    ComPort = $devComPort
                    Line = $lineText
                }
                $index++
            }
        }
        Write-Host ""
        return $deviceList
    }
    
    if ($BusId) {
        # User specified bus ID directly
        $jlinkLine = $jlinkDevices | Where-Object { $_.Line -match "^\s*$BusId\s+" } | Select-Object -First 1
        if ($jlinkLine) {
            $targetBusId = $BusId
        } else {
            Write-Host "Error: Bus ID $BusId not found!" -ForegroundColor Red
            exit 1
        }
    } elseif ($ComPort) {
        # User specified COM port
        $jlinkLine = $jlinkDevices | Where-Object { $_.Line -match $ComPort } | Select-Object -First 1
        if ($jlinkLine -and $jlinkLine.Line -match "^\s*([0-9-]+)") {
            $targetBusId = $matches[1]
        } else {
            Write-Host "Error: COM port $ComPort not found!" -ForegroundColor Red
            exit 1
        }
    } elseif ($NodeType) {
        # NodeType specified - show menu to select
        if ($jlinkDevices.Count -gt 1) {
            $deviceList = Show-DeviceMenu -Devices $jlinkDevices -Prompt "Multiple J-Link devices found. Please select which $NodeType board to flash:"
            
            if ($deviceList.Count -eq 0) {
                Write-Host "Error: No devices could be parsed from usbipd output!" -ForegroundColor Red
                Write-Host "  Raw output:" -ForegroundColor Gray
                $jlinkDevices | ForEach-Object { Write-Host "    $($_.Line)" -ForegroundColor Gray }
                exit 1
            }
            
            $selection = Read-Host "Select device number (1-$($deviceList.Count)) or press Enter for first device"
            
            if ($selection -match "^\d+$" -and [int]$selection -ge 1 -and [int]$selection -le $deviceList.Count) {
                $selectedDevice = $deviceList[[int]$selection - 1]
                $targetBusId = $selectedDevice.BusId
                if (-not $targetBusId) {
                    Write-Host "Error: Could not extract Bus ID from selected device!" -ForegroundColor Red
                    Write-Host "  Device line: $($selectedDevice.Line)" -ForegroundColor Gray
                    Write-Host "  Device object: $($selectedDevice | ConvertTo-Json)" -ForegroundColor Gray
                    exit 1
                }
                # Find matching line in original jlinkDevices
                $lineText = if ($selectedDevice.Line) { $selectedDevice.Line } else { "" }
                $jlinkLine = $jlinkDevices | Where-Object { 
                    $devLine = if ($_.Line) { $_.Line } else { $_.ToString() }
                    $devLine -match "^\s*$targetBusId\s+" -or $devLine -match "$targetBusId"
                } | Select-Object -First 1
                $comPort = if ($selectedDevice.ComPort) { $selectedDevice.ComPort } else { "N/A" }
                Write-Host "Selected: Bus ID $targetBusId (COM $comPort)" -ForegroundColor Green
            } else {
                # Default to first device
                if ($deviceList.Count -gt 0) {
                    $selectedDevice = $deviceList[0]
                    $targetBusId = $selectedDevice.BusId
                    $jlinkLine = $jlinkDevices | Where-Object { 
                        $devLine = if ($_.Line) { $_.Line } else { $_.ToString() }
                        $devLine -match "$targetBusId"
                    } | Select-Object -First 1
                    Write-Host "Using first device: Bus ID $targetBusId" -ForegroundColor Yellow
                } elseif ($jlinkDevices.Count -gt 0) {
                    $lineText = if ($jlinkDevices[0].Line) { $jlinkDevices[0].Line } else { $jlinkDevices[0].ToString() }
                    if ($lineText -match "^\s*([0-9-]+)") {
                        $targetBusId = $matches[1]
                        $jlinkLine = $jlinkDevices[0]
                        Write-Host "Using first device: Bus ID $targetBusId" -ForegroundColor Yellow
                    }
                }
            }
        } else {
            # Only one device
            $jlinkLine = $jlinkDevices | Select-Object -First 1
            if ($jlinkLine -and $jlinkLine.Line -match "^\s*([0-9-]+)") {
                $targetBusId = $matches[1]
                Write-Host "Found J-Link device: Bus ID $targetBusId" -ForegroundColor Green
            }
        }
    } else {
        # No parameters - show selection menu if multiple devices
        if ($jlinkDevices.Count -gt 1) {
            $deviceList = Show-DeviceMenu -Devices $jlinkDevices -Prompt "Multiple J-Link devices found. Please select which board to flash:"
            $selection = Read-Host "Select device number (1-$($deviceList.Count)) or press Enter for first device"
            
            if ($selection -match "^\d+$" -and [int]$selection -ge 1 -and [int]$selection -le $deviceList.Count) {
                $selectedDevice = $deviceList[[int]$selection - 1]
                $targetBusId = $selectedDevice.BusId
                if (-not $targetBusId) {
                    Write-Host "Error: Could not extract Bus ID from selected device!" -ForegroundColor Red
                    Write-Host "  Device line: $($selectedDevice.Line)" -ForegroundColor Gray
                    exit 1
                }
                $jlinkLine = $jlinkDevices | Where-Object { $_.Line -match "^\s*$targetBusId\s+" } | Select-Object -First 1
                if (-not $jlinkLine) {
                    # Try without leading whitespace requirement
                    $jlinkLine = $jlinkDevices | Where-Object { $_.Line -match "$targetBusId" } | Select-Object -First 1
                }
                $comPort = if ($selectedDevice.ComPort) { $selectedDevice.ComPort } else { "N/A" }
                Write-Host "Selected: Bus ID $targetBusId (COM $comPort)" -ForegroundColor Green
            } else {
                # Default to first device
                if ($deviceList.Count -gt 0) {
                    $selectedDevice = $deviceList[0]
                    $targetBusId = $selectedDevice.BusId
                    $jlinkLine = $jlinkDevices | Where-Object { $_.Line -match "$targetBusId" } | Select-Object -First 1
                    Write-Host "Using first device: Bus ID $targetBusId" -ForegroundColor Yellow
                } elseif ($jlinkDevices.Count -gt 0 -and $jlinkDevices[0].Line -match "^\s*([0-9-]+)") {
                    $targetBusId = $matches[1]
                    $jlinkLine = $jlinkDevices[0]
                    Write-Host "Using first device: Bus ID $targetBusId" -ForegroundColor Yellow
                }
            }
        } else {
            # Only one device, use it
            $jlinkLine = $jlinkDevices | Select-Object -First 1
            if ($jlinkLine -and $jlinkLine.Line -match "^\s*([0-9-]+)") {
                $targetBusId = $matches[1]
                Write-Host "Found J-Link device: Bus ID $targetBusId" -ForegroundColor Green
            }
        }
    }
    
    if (-not $targetBusId -or -not $jlinkLine) {
        Write-Host ""
        Write-Host "Could not determine which device to flash!" -ForegroundColor Red
        exit 1
    }
    
    # Check if device needs to be attached to WSL2
    Write-Host "Checking if J-Link device is shared with WSL2..." -ForegroundColor Cyan
    $sharedDevices = wsl bash -c "ls -la /dev/bus/usb 2>&1"
    
    if ($LASTEXITCODE -ne 0 -or $sharedDevices -match "No such file") {
        Write-Host "USB devices not accessible in WSL2. Attaching device..." -ForegroundColor Yellow
        
        # Check if already attached or shared
        if ($jlinkLine.Line -match "Attached|Shared") {
            Write-Host "J-Link device at bus ID $targetBusId is already attached to WSL2 ✓" -ForegroundColor Green
        } else {
            Write-Host "Found J-Link device at bus ID: $targetBusId" -ForegroundColor Cyan
            Write-Host "Attaching to WSL2..." -ForegroundColor Yellow
            
            # New syntax: usbipd attach --wsl --busid <BUSID>
            & $usbipdExe attach --wsl --busid $targetBusId 2>&1 | Out-Null
            
            if ($LASTEXITCODE -ne 0) {
                Write-Host ""
                Write-Host "Failed to attach device automatically." -ForegroundColor Yellow
                Write-Host "Please run setup script (may need admin ONCE):" -ForegroundColor Yellow
                Write-Host "  .\setup-usb.ps1" -ForegroundColor White
                Write-Host ""
                Write-Host "Or attach manually:" -ForegroundColor Yellow
                Write-Host "  usbipd attach --wsl --busid $targetBusId" -ForegroundColor White
                exit 1
            }
            
            Write-Host "Device attached successfully!" -ForegroundColor Green
            Start-Sleep -Seconds 1  # Give WSL2 time to see the device
        }
    } else {
        Write-Host "USB devices are accessible in WSL2 ✓" -ForegroundColor Green
    }
    
    Write-Host "Flashing via WSL2 (no admin required)..." -ForegroundColor Green
    
    # Convert Windows path to WSL2 path
    $wslPath = $projectPath.Replace('C:', '/mnt/c').Replace('\', '/')
    
    # CRITICAL: Detach ALL J-Link devices first, then reattach only the target
    # This ensures a clean state and prevents nrfjprog from seeing multiple devices
    Write-Host "Isolating target device..." -ForegroundColor Cyan
    
    # Step 1: Detach ALL J-Link devices (including target)
    Write-Host "  Step 1: Detaching all J-Link devices..." -ForegroundColor Gray
    $allJlinkDevices = $usbDevices | Select-String -Pattern "SEGGER|J-Link|1366" -CaseSensitive:$false
    foreach ($device in $allJlinkDevices) {
        if ($device.Line -match "^\s*([0-9-]+)") {
            $devBusId = $matches[1]
            if ($device.Line -match "Attached|Shared") {
                Write-Host "    Detaching: Bus ID $devBusId" -ForegroundColor DarkGray
                & $usbipdExe detach --busid $devBusId 2>&1 | Out-Null
            }
        }
    }
    
    # Wait for detachments to complete
    Write-Host "  Waiting for detachments to complete..." -ForegroundColor Gray
    Start-Sleep -Seconds 2
    
    # Step 2: Reattach ONLY the target device
    Write-Host "  Step 2: Attaching target device (Bus ID $targetBusId)..." -ForegroundColor Gray
    & $usbipdExe attach --wsl --busid $targetBusId 2>&1 | Out-Null
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to attach target device!" -ForegroundColor Red
        Write-Host "  Try running: usbipd attach --wsl --busid $targetBusId" -ForegroundColor Yellow
        exit 1
    }
    
    # Wait for WSL2 to recognize the device
    Write-Host "  Waiting for WSL2 to recognize device..." -ForegroundColor Gray
    Start-Sleep -Seconds 4
    
    # Verify target device is attached
    $currentDevices = & $usbipdExe list 2>&1
    $targetDeviceStatus = $currentDevices | Select-String -Pattern "^\s*$targetBusId\s+" | Select-Object -First 1
    
    if ($targetDeviceStatus -and $targetDeviceStatus.Line -match "Attached|Shared") {
        Write-Host "Target device attached successfully ✓" -ForegroundColor Green
    } else {
        Write-Host "Warning: Target device status unclear. Proceeding anyway..." -ForegroundColor Yellow
    }
    
    # Verify only one device is visible to nrfjprog
    Write-Host "Verifying device availability..." -ForegroundColor Cyan
    $nrfjprogOutput = wsl bash -c "cd '$wslPath' && docker run --privileged -v /dev/bus/usb:/dev/bus/usb uberi/qorvo-nrf52833-board nrfjprog --ids 2>&1"
    
    if ($LASTEXITCODE -eq 0 -and $nrfjprogOutput) {
        # Parse serial numbers from nrfjprog output
        # Output format: one serial number per line, e.g. "760201599"
        $serialNumbers = @()
        $lines = $nrfjprogOutput -split "[\r\n]+" | Where-Object { $_.Trim() -ne "" }
        
        Write-Host "  Parsing nrfjprog output ($($lines.Count) lines)..." -ForegroundColor Gray
        foreach ($line in $lines) {
            $trimmed = $line.Trim()
            # Match full serial numbers (typically 9 digits, minimum 6)
            if ($trimmed -match "^\d{6,}$") {
                $serialNumbers += $trimmed
                Write-Host "  Found serial number: $trimmed" -ForegroundColor Gray
            } else {
                Write-Host "  Skipping line (not a serial): '$trimmed'" -ForegroundColor DarkGray
            }
        }
        
        if ($serialNumbers.Count -eq 0) {
            Write-Host "Error: No devices found! Check USB connection." -ForegroundColor Red
            Write-Host "  nrfjprog output: $nrfjprogOutput" -ForegroundColor Gray
            exit 1
        } elseif ($serialNumbers.Count -eq 1) {
            $targetSerial = $serialNumbers[0]
            Write-Host "Found target device serial: $targetSerial" -ForegroundColor Green
            Write-Host "Flashing to device serial $targetSerial (Bus ID $targetBusId)..." -ForegroundColor Cyan
            
            # Flash with specific serial number
            wsl bash -c "cd '$wslPath' && docker run --privileged -v /dev/bus/usb:/dev/bus/usb -v '$wslPath/Output:/project/Output:ro' uberi/qorvo-nrf52833-board nrfjprog --snr $targetSerial --force -f nrf52 --program /project/Output/Common/Exe/dw3000_api.hex --sectorerase --verify"
        } else {
            Write-Host "Warning: Multiple devices still visible ($($serialNumbers.Count))!" -ForegroundColor Yellow
            Write-Host "  Serial numbers found: $($serialNumbers -join ', ')" -ForegroundColor Gray
            Write-Host "  This should not happen. Attempting to flash first device..." -ForegroundColor Yellow
            $targetSerial = $serialNumbers[0]
            Write-Host "  Using serial: $targetSerial" -ForegroundColor Gray
            wsl bash -c "cd '$wslPath' && docker run --privileged -v /dev/bus/usb:/dev/bus/usb -v '$wslPath/Output:/project/Output:ro' uberi/qorvo-nrf52833-board nrfjprog --snr $targetSerial --force -f nrf52 --program /project/Output/Common/Exe/dw3000_api.hex --sectorerase --verify"
        }
    } else {
        Write-Host "Error: Could not enumerate devices!" -ForegroundColor Red
        Write-Host "  nrfjprog output: $nrfjprogOutput" -ForegroundColor Gray
        Write-Host "  Check USB connection and WSL2 device sharing." -ForegroundColor Yellow
        exit 1
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "Flash failed. USB device not accessible." -ForegroundColor Red
        Write-Host ""
        Write-Host "Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  1. Run setup script: .\setup-usb.ps1" -ForegroundColor White
        Write-Host "  2. Verify device is connected and powered on" -ForegroundColor White
        Write-Host "  3. Try unplugging and replugging USB cable" -ForegroundColor White
        Write-Host "  4. Check if device is attached: usbipd list" -ForegroundColor White
        exit 1
    }
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "Flash completed successfully!" -ForegroundColor Green
    Write-Host "Firmware is now running on the board." -ForegroundColor Cyan
} else {
    Write-Host "Flash failed!" -ForegroundColor Red
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Ensure board is connected via USB (J9 port)" -ForegroundColor Yellow
    Write-Host "  2. Ensure board is powered on" -ForegroundColor Yellow
    Write-Host "  3. Try unplugging and replugging the USB cable" -ForegroundColor Yellow
    Write-Host "  4. On Windows, ensure Docker Desktop uses WSL2 backend" -ForegroundColor Yellow
    exit 1
}

