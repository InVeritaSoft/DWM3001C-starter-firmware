# PuTTY Serial Input Troubleshooting

## Common Issues and Fixes

### Issue: Can't Type in PuTTY

**Fix 1: Enable Local Echo**
1. Right-click on PuTTY window title bar
2. Select **Change Settings**
3. Go to **Terminal** → **Local echo**
4. Select **Force on**
5. Click **Apply**

**Fix 2: Check Terminal Settings**
1. Right-click → **Change Settings**
2. Go to **Terminal** → **Keyboard**
3. Make sure **Backspace sends Ctrl+H** is unchecked
4. Make sure **Home and End** are set to **Standard**

**Fix 3: Check Serial Settings**
1. Right-click → **Change Settings**
2. Go to **Connection** → **Serial**
3. Verify:
   - Speed: **115200**
   - Data bits: **8**
   - Stop bits: **1**
   - Parity: **None**
   - Flow control: **None**

**Fix 4: Try Different Terminal Type**
1. Right-click → **Change Settings**
2. Go to **Connection** → **Data**
3. Terminal-type string: Try **xterm** or **vt100**

### Issue: Characters Not Being Sent

**Check if port is in use:**
- Close any other programs using the COM port
- Check Task Manager for other serial port programs

**Try typing and pressing Enter:**
- Type: `PNG`
- Press **Enter** (not just type)
- You should see the command and response

### Issue: No Response

**If you can type but get no response:**
1. Check firmware is running (you should see startup messages)
2. Verify baud rate matches (115200)
3. Try sending: `PNG` + Enter
4. Wait a moment for response

## Alternative: Use Python Script Instead

If PuTTY isn't working, use the Python monitor:

**Windows:**
```powershell
python Zephyr/tests/serial_monitor_simple.py COM3
```

**Linux:**
```bash
python3 Zephyr/tests/serial_monitor.py /dev/ttyUSB0
```

Then send commands from another terminal or script.

## Alternative: PowerShell Serial

**Send commands via PowerShell:**
```powershell
$port = [System.IO.Ports.SerialPort]::new("COM3", 115200, None, 8, One)
$port.Open()
$port.WriteLine("PNG")
Start-Sleep -Milliseconds 500
$response = $port.ReadExisting()
Write-Host "Response: $response"
$port.Close()
```

## Alternative: Tera Term

If PuTTY doesn't work, try Tera Term:
1. Download: https://ttssh2.osdn.jp/index.html.en
2. New connection → Serial → Select COM port
3. Configure: 115200, 8N1
4. Should work better for serial input

## Quick Test

**1. In PuTTY, type:**
```
PNG
```
(Press Enter after typing PNG)

**2. You should see:**
```
PNG
OK
```

**3. If nothing appears:**
- Check Local Echo is ON
- Try typing in the window (not just the title bar)
- Make sure you're clicking in the terminal area
- Try pressing Enter multiple times

## Verify Connection

**Check if you're receiving data:**
1. Connect PuTTY
2. Power cycle the board
3. You should see startup messages immediately
4. If you see messages, connection is working
5. If you can't type, it's an input/terminal issue
