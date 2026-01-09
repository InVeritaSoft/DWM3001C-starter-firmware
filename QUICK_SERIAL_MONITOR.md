# Quick Serial Monitor Guide

## For Linux (if using /dev/ttyUSB0)

### Simplest: Python Script
```bash
python3 Zephyr/tests/serial_monitor.py /dev/ttyUSB0
```

### Or: screen
```bash
screen /dev/ttyUSB0 115200
# Press Ctrl+A then K to exit
```

### Or: minicom
```bash
minicom -D /dev/ttyUSB0 -b 115200
# Press Ctrl+A then X to exit
```

## For Windows

### Option 1: PuTTY (Recommended)
1. Download: https://www.putty.org/
2. Connection type: **Serial**
3. Serial line: **COM3** (check Device Manager for your port)
4. Speed: **115200**
5. Click **Open**

### Option 2: Python (if installed)
```powershell
python Zephyr/tests/serial_monitor_simple.py COM3
```

## Find Your Port

**Windows:**
- Device Manager → Ports (COM & LPT)
- Look for "USB Serial Port" or similar

**Linux:**
```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
```

## What You Should See

### Orchestrator v2 Firmware:
```
OK STARTUP V2
UART INIT OK
[DBG] UART init succeeded
[DBG] UART PSEL: RXD=15 TXD=14 RTS=4 CTS=5 ENABLE=4
```

### Test Command:
Type `PNG` and press Enter, you should see:
```
PNG
OK
```

## Two Terminal Setup

**Terminal 1 - Monitor:**
```bash
# Linux
python3 Zephyr/tests/serial_monitor.py /dev/ttyUSB0

# Windows (PuTTY)
# Open PuTTY, connect to COM port
```

**Terminal 2 - Send Commands:**
```bash
# Linux
python3 Zephyr/tests/test_png_continuous.py /dev/ttyUSB0

# Windows PowerShell
$port = [System.IO.Ports.SerialPort]::new("COM3", 115200)
$port.Open()
$port.WriteLine("PNG")
Start-Sleep -Milliseconds 500
Write-Host $port.ReadExisting()
$port.Close()
```

## Troubleshooting

**No output:**
- Check port number/name
- Verify baud rate is 115200
- Make sure device is connected
- Check if another program is using the port

**Garbled text:**
- Wrong baud rate
- Wrong port
- Hardware issue
