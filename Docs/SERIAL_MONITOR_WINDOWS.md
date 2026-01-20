# Serial Monitor for Windows

## Option 1: PuTTY (Easiest)

1. **Download PuTTY:** https://www.putty.org/
2. **Configure:**
   - Connection type: **Serial**
   - Serial line: `COM3` (or your COM port - check Device Manager)
   - Speed: **115200**
   - Data bits: **8**
   - Stop bits: **1**
   - Parity: **None**
   - Flow control: **None**
3. **Click "Open"**
4. **To exit:** Close window or press `Ctrl+]`

## Option 2: Tera Term

1. **Download Tera Term:** https://ttssh2.osdn.jp/index.html.en
2. **Open:** New connection → Serial → Select COM port
3. **Configure:**
   - Baud rate: **115200**
   - Data: **8 bit**
   - Parity: **none**
   - Stop: **1 bit**
   - Flow control: **none**
4. **Click OK**

## Option 3: Python (if available)

**If you have Python installed:**
```powershell
python Zephyr/tests/serial_monitor.py COM3
```

**Or install pyserial:**
```powershell
pip install pyserial
python Zephyr/tests/serial_monitor.py COM3
```

## Option 4: WSL (Windows Subsystem for Linux)

**If you're using WSL and the device shows as /dev/ttyUSB0:**

```bash
# In WSL terminal
python3 Zephyr/tests/serial_monitor.py /dev/ttyUSB0
```

**Or use screen:**
```bash
screen /dev/ttyUSB0 115200
```

## Finding Your COM Port

**Windows:**
1. Open Device Manager
2. Expand "Ports (COM & LPT)"
3. Look for:
   - USB Serial Port (COMx) - This is your RS-485 adapter
   - J-Link CDC UART (COMx) - This is for debug, NOT commands

**Or use PowerShell:**
```powershell
Get-WmiObject Win32_SerialPort | Select-Object DeviceID, Description
```

## What to Look For

### Orchestrator v2 Firmware:
```
OK STARTUP V2
UART INIT OK
[DBG] UART init succeeded
```

### When you type PNG:
```
PNG
OK
```

## Testing Commands in PuTTY

1. **Open PuTTY** and connect to serial port
2. **Type:** `PNG` and press Enter
3. **You should see:** `OK` response

## Two Windows Setup

**Window 1 - Monitor (PuTTY):**
- Connect to COM port
- Watch for all output

**Window 2 - Send Commands:**
- Use your test script
- Or use PowerShell:
```powershell
$port = [System.IO.Ports.SerialPort]::new("COM3", 115200)
$port.Open()
$port.WriteLine("PNG")
Start-Sleep -Milliseconds 500
$response = $port.ReadExisting()
Write-Host $response
$port.Close()
```
