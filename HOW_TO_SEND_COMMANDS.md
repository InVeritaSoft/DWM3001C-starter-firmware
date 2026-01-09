# How to Send Commands to Serial Monitor

## The Problem

The serial monitor (`serial-monitor.ps1`) is **READ-ONLY** - it only displays data from the device. You **cannot** type commands into it.

## The Solution: Use Two Terminals

### Terminal 1: Monitor (Read Only)
```powershell
.\Scripts\serial-monitor.ps1 COM15
```
This shows all output from the device.

### Terminal 2: Send Commands
```powershell
.\Scripts\send-command.ps1 COM15 PNG
```
This sends commands to the device.

## Step-by-Step Guide

### Method 1: Two PowerShell Windows

**Window 1 - Monitor:**
```powershell
PS> .\Scripts\serial-monitor.ps1 COM15
✓ Connected to COM15
Waiting for data...
```

**Window 2 - Send Commands:**
```powershell
PS> .\Scripts\send-command.ps1 COM15 PNG
✓ Response: OK
```

### Method 2: One Terminal, Sequential

**Step 1:** Start monitor, wait for output, then press `Ctrl+C` to stop
```powershell
PS> .\Scripts\serial-monitor.ps1 COM15
[14:30:15] OK STARTUP V2
[14:30:15] OK MAIN_LOOP
^C  (Press Ctrl+C to stop)
```

**Step 2:** Send command
```powershell
PS> .\Scripts\send-command.ps1 COM15 PNG
✓ Response: OK
```

**Step 3:** Restart monitor to see response
```powershell
PS> .\Scripts\serial-monitor.ps1 COM15
```

## Quick Command Reference

| Command | What It Does |
|---------|--------------|
| `.\Scripts\send-command.ps1 COM15 PNG` | Send PNG command |
| `.\Scripts\send-command.ps1 COM15 START` | Send START command |
| `.\Scripts\send-command.ps1 COM15 STOP` | Send STOP command |
| `.\Scripts\send-command.ps1 COM15 STATS` | Get statistics |

## Alternative: Interactive Command Sender

If you want to send multiple commands interactively, you can use this:

```powershell
# Interactive command sender
$port = New-Object System.IO.Ports.SerialPort COM15, 115200, None, 8, one
$port.Open()
while ($true) {
    $cmd = Read-Host "Enter command (or 'quit' to exit)"
    if ($cmd -eq 'quit') { break }
    $port.WriteLine($cmd)
    Start-Sleep -Milliseconds 500
    if ($port.BytesToRead -gt 0) {
        Write-Host "Response: $($port.ReadExisting())"
    }
}
$port.Close()
```

## Why You See "PNG" in Monitor

When you type in the monitor window, you're just typing text - it's not being sent to the device. The monitor only READS from the serial port.

To actually send commands, you need to use the `send-command.ps1` script in a separate terminal.
