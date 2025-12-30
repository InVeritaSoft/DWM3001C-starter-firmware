# Stream RTT debug logs from both DWM3001CDK nodes via J-Link
# Usage: .\stream-debug-logs.ps1 [NodeASerial] [NodeBSerial]
# Examples:
#   .\stream-debug-logs.ps1
#   .\stream-debug-logs.ps1 760201599 760201600

param(
    [string]$NodeASerial = "",
    [string]$NodeBSerial = ""
)

$ErrorActionPreference = "Stop"

try {
    Write-Host "Streaming RTT debug logs from both DWM3001CDK nodes..." -ForegroundColor Green
    Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
    Write-Host ""
    
    $projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
    $outputPath = Join-Path $projectRoot "Output"
    
    # Ensure Output directory exists
    if (-not (Test-Path $outputPath)) {
        New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
    }
    
    $debugLogFileNameA = "debug-log-node-a.txt"
    $debugLogFileNameB = "debug-log-node-b.txt"
    $debugLogPathA = Join-Path $outputPath $debugLogFileNameA
    $debugLogPathB = Join-Path $outputPath $debugLogFileNameB
    
    # Convert Windows path to WSL2 path format
    $wslPath = $projectRoot.Replace('C:', '/mnt/c').Replace('\', '/')
    
    Write-Host "Debug logs will be written to:" -ForegroundColor Cyan
    Write-Host "  Node A: $debugLogPathA" -ForegroundColor Cyan
    Write-Host "  Node B: $debugLogPathB" -ForegroundColor Cyan
    Write-Host ""
    
    # Check if USB devices are accessible in WSL2
    Write-Host "Checking USB device access..." -ForegroundColor Yellow
    $usbCheck = wsl bash -c "ls -la /dev/bus/usb 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "WARNING: USB devices not accessible in WSL2." -ForegroundColor Yellow
        Write-Host "You may need to share the J-Link USB devices with WSL2:" -ForegroundColor Yellow
        Write-Host "  1. Install usbipd-win: winget install usbipd-win" -ForegroundColor Gray
        Write-Host "  2. List devices: usbipd list" -ForegroundColor Gray
        Write-Host "  3. Attach J-Link devices: usbipd attach --wsl --busid <busid>" -ForegroundColor Gray
        Write-Host ""
        Write-Host "Or run: .\setup-usb.ps1" -ForegroundColor Cyan
        exit 1
    }
    
    Write-Host "USB devices accessible in WSL2 ✓" -ForegroundColor Green
    Write-Host ""
    
    # Find J-Link devices
    Write-Host "Detecting J-Link devices..." -ForegroundColor Yellow
    
    # Check if usbipd is available
    $usbipdExe = Get-Command usbipd -ErrorAction SilentlyContinue
    if (-not $usbipdExe) {
        $usbipdExe = "$env:ProgramFiles\usbipd-win\usbipd.exe"
        if (-not (Test-Path $usbipdExe)) {
            Write-Host "usbipd not found. Please run: .\setup-usb.ps1" -ForegroundColor Red
            exit 1
        }
    } else {
        $usbipdExe = $usbipdExe.Source
    }
    
    # Check if J-Link devices are attached to WSL2
    Write-Host "Checking for J-Link devices in WSL2..." -ForegroundColor Yellow
    $usbipdList = & $usbipdExe list 2>&1
    $jlinkDevices = $usbipdList | Select-String -Pattern "SEGGER|J-Link|1366" -CaseSensitive:$false
    
    if ($jlinkDevices.Count -eq 0) {
        Write-Host "No J-Link devices found in usbipd list!" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please attach J-Link devices to WSL2:" -ForegroundColor Yellow
        Write-Host "  1. Run: usbipd list" -ForegroundColor White
        Write-Host "  2. Find your J-Link devices (look for SEGGER or VID 1366)" -ForegroundColor White
        Write-Host "  3. Attach each device: usbipd attach --wsl --busid <busid>" -ForegroundColor White
        Write-Host ""
        Write-Host "Or run: .\setup-usb.ps1" -ForegroundColor Cyan
        exit 1
    }
    
    Write-Host "Found $($jlinkDevices.Count) J-Link device(s) in usbipd list" -ForegroundColor Green
    $jlinkDevices | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    Write-Host ""
    
    # Get J-Link serial numbers via nrfjprog
    Write-Host "Running nrfjprog to detect devices..." -ForegroundColor Yellow
    $nrfjprogOutput = wsl bash -c "cd '$wslPath' && docker run --privileged -v /dev/bus/usb:/dev/bus/usb uberi/qorvo-nrf52833-board nrfjprog --ids 2>&1"
    $nrfjprogExitCode = $LASTEXITCODE
    
    Write-Host "nrfjprog exit code: $nrfjprogExitCode" -ForegroundColor Gray
    if ($nrfjprogOutput) {
        Write-Host "nrfjprog output:" -ForegroundColor Gray
        $nrfjprogOutput | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    } else {
        Write-Host "nrfjprog returned no output" -ForegroundColor Gray
    }
    Write-Host ""
    
    # Try alternative method: Extract serial numbers from Windows Device Manager
    if ($nrfjprogExitCode -ne 0 -or -not $nrfjprogOutput -or ($nrfjprogOutput -match "^\s*$")) {
        Write-Host "nrfjprog did not return serial numbers. Trying Windows Device Manager..." -ForegroundColor Yellow
        
        # Extract serial numbers from Windows Device Manager InstanceId
        # Format: USB\VID_1366&PID_0101\000760201599
        $pnpDevices = Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object { 
            $_.FriendlyName -like "*J-Link*" -or $_.FriendlyName -like "*SEGGER*" 
        }
        
        $foundSerials = @()
        foreach ($device in $pnpDevices) {
            if ($device.InstanceId -match '\\000(\d{9})$') {
                $serial = $matches[1]
                $foundSerials += $serial
                Write-Host "  Found serial: $serial (from $($device.FriendlyName))" -ForegroundColor Gray
            }
        }
        
        if ($foundSerials.Count -gt 0) {
            Write-Host "Extracted $($foundSerials.Count) serial number(s) from Device Manager" -ForegroundColor Green
            $nrfjprogOutput = $foundSerials -join "`n"
        } else {
            Write-Host "Could not extract serial numbers from Device Manager" -ForegroundColor Yellow
        }
    }
    
    # Parse serial numbers from nrfjprog output
    $serialNumbers = @()
    if ($nrfjprogOutput -and ($nrfjprogOutput -notmatch "^\s*$")) {
        $lines = $nrfjprogOutput -split "[\r\n]+" | Where-Object { $_.Trim() -ne "" }
        foreach ($line in $lines) {
            $trimmed = $line.Trim()
            if ($trimmed -match "^\d{6,}$") {
                $serialNumbers += $trimmed
            }
        }
    }
    
    # If no serials found and user provided them, use those
    if ($serialNumbers.Count -eq 0) {
        if ($NodeASerial -and $NodeBSerial) {
            Write-Host "Using manually specified serial numbers..." -ForegroundColor Green
            $serialNumbers = @($NodeASerial, $NodeBSerial)
        } elseif ($NodeASerial) {
            Write-Host "Using manually specified Node A serial..." -ForegroundColor Green
            $serialNumbers = @($NodeASerial)
        } else {
            Write-Host "Warning: Could not automatically detect device serial numbers!" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "nrfjprog may not detect devices if firmware is not flashed." -ForegroundColor Gray
            Write-Host ""
            Write-Host "Options:" -ForegroundColor Cyan
            Write-Host "  1. Manually specify serial numbers:" -ForegroundColor White
            Write-Host "     .\stream-debug-logs.ps1 -NodeASerial <serial1> -NodeBSerial <serial2>" -ForegroundColor Gray
            Write-Host ""
            Write-Host "  2. Find serial numbers:" -ForegroundColor White
            Write-Host "     - Check the label on your J-Link device" -ForegroundColor Gray
            Write-Host "     - Or check Device Manager → J-Link devices → Properties → Details → Device Instance ID" -ForegroundColor Gray
            Write-Host ""
            Write-Host "  3. Try with single device first:" -ForegroundColor White
            Write-Host "     .\stream-debug-logs.ps1 -NodeASerial <serial>" -ForegroundColor Gray
            Write-Host ""
            exit 1
        }
    }
    
    Write-Host "Found $($serialNumbers.Count) J-Link device(s):" -ForegroundColor Green
    foreach ($sn in $serialNumbers) {
        Write-Host "  - Serial: $sn" -ForegroundColor Cyan
    }
    Write-Host ""
    
    # Determine which serial is Node A and which is Node B
    $nodeASN = $null
    $nodeBSN = $null
    
    if ($serialNumbers.Count -eq 1) {
        Write-Host "Only one device found. Will stream from single device." -ForegroundColor Yellow
        $nodeASN = $serialNumbers[0]
    } elseif ($serialNumbers.Count -eq 2) {
        if ($NodeASerial -and $NodeBSerial) {
            # User specified serials
            if ($serialNumbers -contains $NodeASerial -and $serialNumbers -contains $NodeBSerial) {
                $nodeASN = $NodeASerial
                $nodeBSN = $NodeBSerial
            } else {
                Write-Host "Error: Specified serial numbers not found!" -ForegroundColor Red
                exit 1
            }
        } else {
            # Auto-assign: first = Node A, second = Node B
            $nodeASN = $serialNumbers[0]
            $nodeBSN = $serialNumbers[1]
            Write-Host "Auto-assigning devices:" -ForegroundColor Yellow
            Write-Host "  Node A: Serial $nodeASN" -ForegroundColor Cyan
            Write-Host "  Node B: Serial $nodeBSN" -ForegroundColor Cyan
            Write-Host ""
            Write-Host "To specify manually, use:" -ForegroundColor Gray
            Write-Host "  .\stream-debug-logs.ps1 -NodeASerial $($serialNumbers[0]) -NodeBSerial $($serialNumbers[1])" -ForegroundColor Gray
            Write-Host ""
        }
    } else {
        Write-Host "Multiple devices found. Please specify which are Node A and Node B:" -ForegroundColor Yellow
        Write-Host "  .\stream-debug-logs.ps1 -NodeASerial <serial> -NodeBSerial <serial>" -ForegroundColor White
        exit 1
    }
    
    # Function to run RTT logger for a specific device
    function Start-RTTLogger {
        param(
            [string]$SerialNumber,
            [string]$NodeLabel,
            [string]$LogFile,
            [string]$WslPath
        )
        
        Write-Host "Starting RTT logger for $NodeLabel (Serial: $SerialNumber)..." -ForegroundColor Cyan
        
        # Run RTT logger in background via WSL
        # The logger needs to run continuously and will reconnect automatically
        $logFileName = Split-Path -Leaf $LogFile
        
        # Convert Windows path to WSL path
        $wslOutputPath = $WslPath.Replace('\', '/').Replace('C:', '/mnt/c')
        
        # Start job that runs RTT logger continuously
        # Run docker command directly in a loop within the job
        # Escape the command properly for bash
        $dockerCmd = "docker run --rm --privileged -v /dev/bus/usb:/dev/bus/usb -v '$wslOutputPath/Output:/project/Output' uberi/qorvo-nrf52833-board /usr/local/JLink_Linux_V792n_x86_64/JLinkRTTLogger -Device NRF52833_XXAA -if SWD -Speed 4000 -USB $SerialNumber -RTTChannel 0 /project/Output/$logFileName"
        
        $job = Start-Job -ScriptBlock {
            param($wp, $cmd)
            # Run in a loop - reconnect if logger exits
            wsl bash -c "cd '$wp' && while true; do $cmd; sleep 2; done"
        } -ArgumentList $wslOutputPath, $dockerCmd
        
        # Return job info
        return @{
            Job = $job
            Serial = $SerialNumber
            LogFile = $LogFile
        }
    }
    
    # Start RTT loggers
    $loggers = @()
    
    if ($nodeASN) {
        $loggerA = Start-RTTLogger -SerialNumber $nodeASN -NodeLabel "Node A" -LogFile $debugLogPathA -WslPath $wslPath
        $loggers += @{ Job = $loggerA.Job; Label = "[Node A]"; LogFile = $debugLogPathA; Serial = $nodeASN }
        Start-Sleep -Milliseconds 1000  # Delay between starting loggers
    }
    
    if ($nodeBSN) {
        $loggerB = Start-RTTLogger -SerialNumber $nodeBSN -NodeLabel "Node B" -LogFile $debugLogPathB -WslPath $wslPath
        $loggers += @{ Job = $loggerB.Job; Label = "[Node B]"; LogFile = $debugLogPathB; Serial = $nodeBSN }
    }
    
    Write-Host ""
    Write-Host "RTT loggers started:" -ForegroundColor Green
    if ($nodeASN) {
        Write-Host "  [Node A] Serial: $nodeASN → $debugLogFileNameA" -ForegroundColor Cyan
    }
    if ($nodeBSN) {
        Write-Host "  [Node B] Serial: $nodeBSN → $debugLogFileNameB" -ForegroundColor Green
    }
    Write-Host ""
    Write-Host "RTT loggers are starting..." -ForegroundColor Yellow
    
    # Check if firmware is flashed
    $firmwareFile = Join-Path $projectRoot "Output\Common\Exe\dw3000_api.hex"
    if (-not (Test-Path $firmwareFile)) {
        Write-Host ""
        Write-Host "WARNING: Firmware file not found!" -ForegroundColor Yellow
        Write-Host "  Expected: $firmwareFile" -ForegroundColor Gray
        Write-Host ""
        Write-Host "Please build and flash firmware first:" -ForegroundColor Cyan
        Write-Host "  .\build.ps1" -ForegroundColor White
        Write-Host "  .\flash.ps1 -NodeType TX  # For Node A" -ForegroundColor White
        Write-Host "  .\flash.ps1 -NodeType RX  # For Node B" -ForegroundColor White
        Write-Host ""
    }
    
    Write-Host ""
    Write-Host "IMPORTANT INSTRUCTIONS:" -ForegroundColor Cyan
    Write-Host "  1. Make sure firmware is FLASHED on both boards" -ForegroundColor White
    Write-Host "  2. Press RESET button (SW1) on BOTH boards NOW!" -ForegroundColor Yellow
    Write-Host "  3. The RTT logger will connect when firmware starts" -ForegroundColor White
    Write-Host "  4. If no output appears, press reset again" -ForegroundColor White
    Write-Host "  5. Watch for log files: $debugLogFileNameA and $debugLogFileNameB" -ForegroundColor Gray
    Write-Host ""
    Start-Sleep -Seconds 5
    
    # Check if loggers started successfully
    $allStarted = $true
    foreach ($item in $loggers) {
        $jobState = $item.Job.State
        Write-Host "  $($item.Label) job state: $jobState" -ForegroundColor $(if ($jobState -eq "Running") { "Green" } else { "Yellow" })
        
        if ($jobState -ne "Running") {
            $allStarted = $false
            # Get any error output
            $jobOutput = Receive-Job $item.Job -ErrorAction SilentlyContinue
            if ($jobOutput) {
                Write-Host "    Output: $($jobOutput -join "`n")" -ForegroundColor Gray
            }
            # Check if job has any errors
            $jobErrors = $item.Job.ChildJobs[0].Error
            if ($jobErrors) {
                Write-Host "    Errors: $($jobErrors -join "`n")" -ForegroundColor Red
            }
        }
    }
    
    if (-not $allStarted) {
        Write-Host ""
        Write-Host "Some loggers failed to start. Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  1. Check if Docker containers are running:" -ForegroundColor White
        Write-Host "     wsl bash -c 'docker ps'" -ForegroundColor Gray
        Write-Host "  2. Try running RTT logger manually for one device:" -ForegroundColor White
        Write-Host "     wsl bash -c 'cd /mnt/c/... && docker run --privileged -v /dev/bus/usb:/dev/bus/usb -v .../Output:/project/Output uberi/qorvo-nrf52833-board /usr/local/JLink_Linux_V792n_x86_64/JLinkRTTLogger -Device NRF52833_XXAA -if SWD -Speed 4000 -USB <serial> -RTTChannel 0 /project/Output/debug-log.txt'" -ForegroundColor Gray
        Write-Host ""
    }
    
    Write-Host ""
    Write-Host "Monitoring output... (Press Ctrl+C to stop)" -ForegroundColor Yellow
    Write-Host ("=" * 60) -ForegroundColor Gray
    Write-Host ""
    
    # Monitor and display merged output
    $lastPositions = @{}
    foreach ($item in $loggers) {
        $lastPositions[$item.LogFile] = 0
    }
    
    try {
        while ($true) {
            $anyOutput = $false
            
            foreach ($item in $loggers) {
                if (Test-Path $item.LogFile) {
                    $content = Get-Content $item.LogFile -ErrorAction SilentlyContinue
                    if ($content.Count -gt $lastPositions[$item.LogFile]) {
                        $newLines = $content[$lastPositions[$item.LogFile]..($content.Count - 1)]
                        foreach ($line in $newLines) {
                            if ($line.Trim() -ne "") {
                                Write-Host "$($item.Label) $line" -ForegroundColor $(if ($item.Label -eq "[Node A]") { "Cyan" } else { "Green" })
                                $anyOutput = $true
                            }
                        }
                        $lastPositions[$item.LogFile] = $content.Count
                    }
                }
            }
            
            if (-not $anyOutput) {
                Start-Sleep -Milliseconds 100
            }
        }
    } catch {
        # User pressed Ctrl+C or error occurred
        Write-Host "`nStopping RTT loggers..." -ForegroundColor Yellow
    } finally {
        # Stop all jobs
        foreach ($item in $loggers) {
            if ($item.Job -and $item.Job.State -eq "Running") {
                Write-Host "Stopping $($item.Label) logger..." -ForegroundColor Yellow
                Stop-Job $item.Job -ErrorAction SilentlyContinue
                Remove-Job $item.Job -Force -ErrorAction SilentlyContinue
            }
        }
        
        # Kill any remaining Docker containers
        Write-Host "Cleaning up Docker containers..." -ForegroundColor Yellow
        wsl bash -c "docker ps -q --filter 'ancestor=uberi/qorvo-nrf52833-board' | xargs -r docker kill 2>/dev/null" | Out-Null
        
        Write-Host "Done." -ForegroundColor Green
    }
    
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "`nMake sure:" -ForegroundColor Yellow
    Write-Host "  1. Docker Desktop is running with WSL2 backend" -ForegroundColor Yellow
    Write-Host "  2. Both boards are connected via J9 USB ports" -ForegroundColor Yellow
    Write-Host "  3. USB devices are shared with WSL2 (run: .\setup-usb.ps1)" -ForegroundColor Yellow
    Write-Host "  4. Boards are powered on" -ForegroundColor Yellow
    exit 1
}
