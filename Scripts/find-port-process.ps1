# Find what process is using a COM port
# Usage: .\find-port-process.ps1 COM20

param(
    [Parameter(Mandatory=$true)]
    [string]$PortName
)

Write-Host "Finding process using $PortName..." -ForegroundColor Cyan
Write-Host ""

# Ensure port name is uppercase and starts with COM
if (-not $PortName.StartsWith("COM")) {
    $PortName = "COM$PortName"
}
$PortName = $PortName.ToUpper()

# Method 1: Try to use Get-NetTCPConnection (won't work for serial ports, but worth trying)
Write-Host "Method 1: Checking network connections..." -ForegroundColor Yellow
try {
    $connections = Get-NetTCPConnection -ErrorAction SilentlyContinue | Where-Object { $_.LocalAddress -like "*$PortName*" }
    if ($connections) {
        Write-Host "Found network connections (unlikely for serial ports)" -ForegroundColor Green
    }
} catch {
    Write-Host "  Network method not applicable for serial ports" -ForegroundColor Gray
}

# Method 2: Check if port can be opened and list all Python/Node processes
Write-Host ""
Write-Host "Method 2: Checking running processes that might use serial ports..." -ForegroundColor Yellow

# Find Python processes
$pythonProcs = Get-Process python* -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*python*" }
if ($pythonProcs) {
    Write-Host "  Python processes found:" -ForegroundColor Cyan
    foreach ($proc in $pythonProcs) {
        Write-Host "    PID $($proc.Id): $($proc.ProcessName) - $($proc.Path)" -ForegroundColor White
        Write-Host "      Command Line:" -ForegroundColor Gray
        try {
            $cmdLine = (Get-CimInstance Win32_Process -Filter "ProcessId = $($proc.Id)").CommandLine
            if ($cmdLine) {
                Write-Host "        $cmdLine" -ForegroundColor Gray
            }
        } catch {
            Write-Host "        (Could not retrieve command line)" -ForegroundColor DarkGray
        }
    }
} else {
    Write-Host "  No Python processes found" -ForegroundColor Gray
}

# Find Node.js processes
$nodeProcs = Get-Process node* -ErrorAction SilentlyContinue
if ($nodeProcs) {
    Write-Host "  Node.js processes found:" -ForegroundColor Cyan
    foreach ($proc in $nodeProcs) {
        Write-Host "    PID $($proc.Id): $($proc.ProcessName) - $($proc.Path)" -ForegroundColor White
    }
}

# Find PowerShell processes that might be running serial scripts
Write-Host ""
Write-Host "Method 3: Checking PowerShell processes..." -ForegroundColor Yellow
$psProcs = Get-Process powershell* -ErrorAction SilentlyContinue
if ($psProcs) {
    Write-Host "  PowerShell processes found:" -ForegroundColor Cyan
    foreach ($proc in $psProcs) {
        Write-Host "    PID $($proc.Id): $($proc.ProcessName)" -ForegroundColor White
        Write-Host "      Window Title: $($proc.MainWindowTitle)" -ForegroundColor Gray
    }
    Write-Host ""
    Write-Host "  Tip: Check each PowerShell window for serial monitor scripts" -ForegroundColor Yellow
    Write-Host "       Look for scripts like: serial-monitor.ps1, test-all-commands.py" -ForegroundColor Yellow
}

# Method 4: Try to open the port and see what error we get
Write-Host ""
Write-Host "Method 4: Attempting to open $PortName..." -ForegroundColor Yellow
try {
    $port = New-Object System.IO.Ports.SerialPort $PortName, 57600, None, 8, one
    $port.Open()
    Write-Host "  ✓ Port $PortName is AVAILABLE (opened successfully)" -ForegroundColor Green
    $port.Close()
} catch {
    $errorMsg = $_.Exception.Message
    Write-Host "  ✗ Port $PortName is LOCKED" -ForegroundColor Red
    Write-Host "    Error: $errorMsg" -ForegroundColor Red
    
    if ($errorMsg -like "*Access*" -or $errorMsg -like "*denied*" -or $errorMsg -like "*in use*") {
        Write-Host ""
        Write-Host "="*60 -ForegroundColor Cyan
        Write-Host "RECOMMENDED ACTIONS:" -ForegroundColor Yellow
        Write-Host "="*60 -ForegroundColor Cyan
        Write-Host ""
        Write-Host "1. Check all open terminal/PowerShell windows:" -ForegroundColor White
        Write-Host "   - Look for windows running serial-monitor.ps1" -ForegroundColor Gray
        Write-Host "   - Look for windows running test-all-commands.py" -ForegroundColor Gray
        Write-Host "   - Look for windows running orchestrator web server" -ForegroundColor Gray
        Write-Host "   - Press Ctrl+C in any terminal that might be using the port" -ForegroundColor Gray
        Write-Host ""
        Write-Host "2. Check for serial monitor applications:" -ForegroundColor White
        Write-Host "   - PuTTY (check all open PuTTY windows)" -ForegroundColor Gray
        Write-Host "   - Tera Term" -ForegroundColor Gray
        Write-Host "   - Arduino IDE Serial Monitor" -ForegroundColor Gray
        Write-Host "   - Any other serial terminal programs" -ForegroundColor Gray
        Write-Host ""
        Write-Host "3. Check Task Manager:" -ForegroundColor White
        Write-Host "   - Open Task Manager (Ctrl+Shift+Esc)" -ForegroundColor Gray
        Write-Host "   - Look for Python, Node.js, or PowerShell processes" -ForegroundColor Gray
        Write-Host "   - End any processes that might be using the port" -ForegroundColor Gray
        Write-Host ""
        Write-Host "4. Try unplugging and replugging the USB-to-RS485 adapter" -ForegroundColor White
        Write-Host ""
        Write-Host "5. If all else fails, restart your computer" -ForegroundColor White
    }
}

# Method 5: List all available ports for comparison
Write-Host ""
Write-Host "Method 5: Available COM ports:" -ForegroundColor Yellow
$availablePorts = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
if ($availablePorts) {
    foreach ($port in $availablePorts) {
        if ($port -eq $PortName) {
            Write-Host "  $port (TARGET - currently locked)" -ForegroundColor Red
        } else {
            Write-Host "  $port" -ForegroundColor Green
        }
    }
} else {
    Write-Host "  No COM ports found" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "="*60 -ForegroundColor Cyan
Write-Host "QUICK FIX:" -ForegroundColor Yellow
Write-Host "="*60 -ForegroundColor Cyan
Write-Host "1. Close ALL terminal/PowerShell windows" -ForegroundColor White
Write-Host "2. Close ALL serial monitor applications" -ForegroundColor White
Write-Host "3. Unplug and replug the USB-to-RS485 adapter" -ForegroundColor White
Write-Host "4. Run your test again" -ForegroundColor White
Write-Host ""
