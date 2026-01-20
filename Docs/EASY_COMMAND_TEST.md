# Easy Command Testing (No PuTTY Input Issues)

## Problem: Can't Type in PuTTY

If you can't enter commands in PuTTY, use these alternatives:

## Option 1: Python Script (Easiest)

**Windows:**
```powershell
python Zephyr/tests/send_command.py COM3 PNG
```

**Linux:**
```bash
python3 Zephyr/tests/send_command.py /dev/ttyUSB0 PNG
```

This will:
- Connect to the port
- Send the command
- Show the response
- Exit

## Option 2: PowerShell Script (Windows)

```powershell
.\SEND_COMMANDS_WINDOWS.ps1 COM3 PNG
```

Or for other commands:
```powershell
.\SEND_COMMANDS_WINDOWS.ps1 COM3 "NODE_TYPE"
.\SEND_COMMANDS_WINDOWS.ps1 COM3 "STAT"
```

## Option 3: Two-Window Setup

**Window 1 - Monitor (PuTTY):**
- Just watch output (don't need to type)
- Connect to COM port
- See all messages

**Window 2 - Send Commands:**
- Use Python script or PowerShell
- Send commands from here
- Watch responses in Window 1

## Option 4: Fix PuTTY Input

**Enable Local Echo:**
1. Right-click PuTTY title bar
2. **Change Settings**
3. **Terminal** → **Local echo** → **Force on**
4. Click **Apply**

**Or try:**
- Click in the terminal window (not title bar)
- Type and press Enter
- Make sure you're in the terminal area

## Quick Test

**Send PNG command:**
```powershell
# Windows PowerShell
python Zephyr/tests/send_command.py COM3 PNG
```

**Expected output:**
```
Connecting to COM3...
✓ Connected

Sending: PNG
Waiting for response...
Response: OK
```

## Monitor + Send Commands

**Terminal 1 - Monitor (watch only):**
```bash
# Linux
python3 Zephyr/tests/serial_monitor.py /dev/ttyUSB0

# Windows (PuTTY)
# Just open and watch - don't need to type
```

**Terminal 2 - Send commands:**
```bash
# Linux
python3 Zephyr/tests/send_command.py /dev/ttyUSB0 PNG

# Windows PowerShell
python Zephyr/tests/send_command.py COM3 PNG
```

This way you can:
- See all firmware output in Terminal 1
- Send commands from Terminal 2
- Watch responses in Terminal 1
