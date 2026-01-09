# Windows Serial Communication Guide

## Quick Start

### Option 1: PowerShell Scripts (No Python Needed!)

**Monitor serial output:**
```powershell
.\Scripts\serial-monitor.ps1 COM3
```

**Send a command:**
```powershell
.\Scripts\send-command.ps1 COM3 PNG
```

**Send other commands:**
```powershell
.\Scripts\send-command.ps1 COM3 "NODE_TYPE"
.\Scripts\send-command.ps1 COM3 "STAT"
```

### Option 2: Install Python pyserial

**Install:**
```powershell
pip install pyserial
```

**Then use Python scripts:**
```powershell
python Zephyr\tests\send_command.py COM3 PNG
python Zephyr\tests\serial_monitor_simple.py COM3
```

## Find Your COM Port

**PowerShell:**
```powershell
Get-WmiObject Win32_SerialPort | Select-Object DeviceID, Description
```

**Or Device Manager:**
- Open Device Manager
- Expand "Ports (COM & LPT)"
- Look for "USB Serial Port" or similar
- Note the COM number (e.g., COM3, COM4)

## Two-Window Setup

**Window 1 - Monitor (PowerShell):**
```powershell
.\Scripts\serial-monitor.ps1 COM3
```
- Shows all firmware output
- Real-time monitoring
- Press Ctrl+C to exit

**Window 2 - Send Commands (PowerShell):**
```powershell
.\Scripts\send-command.ps1 COM3 PNG
```
- Sends commands
- Shows responses
- Exits automatically

## What You Should See

### Orchestrator v2 Firmware:
```
OK STARTUP V2
UART INIT OK
[DBG] UART init succeeded
[DBG] UART PSEL: RXD=15 TXD=14 RTS=4 CTS=5 ENABLE=4
```

### When you send PNG:
```
Sending: PNG
Waiting for response...
Response: OK
```

## Troubleshooting

### "Access to the port 'COM3' is denied"
- Another program is using the port
- Close PuTTY, other serial monitors, or test scripts
- Try again

### "The port 'COM3' does not exist"
- Check COM port number in Device Manager
- Try different COM ports
- Verify device is connected

### No Response
- Check firmware is running (should see startup messages)
- Verify baud rate is 115200
- Check hardware connections

## Example Session

**Terminal 1:**
```powershell
PS> .\Scripts\serial-monitor.ps1 COM3
============================================================
Serial Monitor - Real-time Firmware Output
============================================================
Port: COM3
Baud: 115200
============================================================
✓ Connected to COM3

Waiting for data...
OK STARTUP V2
UART INIT OK
```

**Terminal 2:**
```powershell
PS> .\Scripts\send-command.ps1 COM3 PNG
Connecting to COM3...
✓ Connected

Sending: PNG
Waiting for response...
Response: OK
```

## All Available Scripts

**In Scripts folder:**
- `serial-monitor.ps1` - Monitor all output
- `send-command.ps1` - Send single command
- `check-current-firmware.ps1` - Check firmware status
- `test-uart-direct.ps1` - Direct UART test

**In Zephyr/tests folder (requires Python):**
- `send_command.py` - Send command (cross-platform)
- `serial_monitor.py` - Monitor output (cross-platform)
- `serial_monitor_simple.py` - Simple monitor
