# TTL Serial Monitor Connection Guide

## Quick Start

### Option 1: PowerShell Serial Monitor (Recommended for Windows)

**Step 1: Find your COM port**
```powershell
.\Scripts\list-com-ports.ps1
```

**Step 2: Connect to serial monitor**
```powershell
.\Scripts\serial-monitor.ps1 COM15
```
(Replace `COM15` with your actual COM port)

**Step 3: Monitor output**
- All serial data will be displayed in real-time
- Press `Ctrl+C` to exit

### Option 2: Python Serial Monitor

**Step 1: Install pyserial (if not already installed)**
```powershell
pip install pyserial
```

**Step 2: Run the monitor**
```powershell
python -c "import serial; import time; port = serial.Serial('COM15', 115200, timeout=1); print('Connected. Press Ctrl+C to exit'); [print(f'[{time.strftime(\"%H:%M:%S\")}] {port.readline().decode(\"utf-8\", errors=\"ignore\").strip()}') for _ in iter(int, 1)]"
```

Or use a simple Python script:
```python
import serial
import time

port = serial.Serial('COM15', 115200, timeout=1)
print("Connected. Press Ctrl+C to exit")

try:
    while True:
        if port.in_waiting > 0:
            line = port.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f'[{time.strftime("%H:%M:%S")}] {line}')
        time.sleep(0.01)
except KeyboardInterrupt:
    port.close()
    print("\nDisconnected")
```

### Option 3: PuTTY (GUI Tool)

**Step 1: Download PuTTY**
- Download from: https://www.putty.org/

**Step 2: Configure PuTTY**
1. Open PuTTY
2. Connection type: **Serial**
3. Serial line: `COM15` (your COM port)
4. Speed: `115200`
5. Click **Open**

**Step 3: Settings (if needed)**
- Terminal → Local echo: **Force on** (to see what you type)
- Terminal → Local line editing: **Force on**

### Option 4: Tera Term (Alternative GUI)

**Step 1: Download Tera Term**
- Download from: https://ttssh2.osdn.jp/

**Step 2: Connect**
1. Open Tera Term
2. Select **Serial**
3. Choose your COM port
4. Baud rate: `115200`
5. Click **OK**

## Connection Parameters

**Standard settings for DWM3001C:**
- **Baud Rate:** 115200
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1
- **Flow Control:** None

## What You Should See

### When Device Starts Up

**Orchestrator v2 firmware:**
```
OK STARTUP V2
UART INIT OK
[DBG] UART init succeeded
[DBG] uart_init entry: rx=15 tx=14 rts=4 cts=5
[DBG] APP_UART_FIFO_INIT result: err=0x00000000
[DBG] UART PSEL: RXD=15 TXD=14 RTS=4 CTS=5 ENABLE=4
OK DW3000_READY
OK MAIN_LOOP
```

### When Sending Commands

**Send:** `PNG`
**Receive:** `OK`

## Troubleshooting

### "Port not found" or "Access denied"

**Solution:**
1. Check COM port number: `.\Scripts\list-com-ports.ps1`
2. Close other programs using the port (PuTTY, Arduino IDE, etc.)
3. Try a different USB cable
4. Unplug and replug the device

### No data appearing

**Check:**
1. Is the device powered on?
2. Is the firmware running? (Look for startup messages)
3. Is the baud rate correct? (115200)
4. Are TX/RX wires connected correctly?

### Garbled characters

**Causes:**
- Wrong baud rate
- Loose connections
- Electrical interference

**Fix:**
- Verify baud rate is exactly 115200
- Check all connections
- Try a different USB cable

## Advanced: Send Commands While Monitoring

**Terminal 1 - Monitor:**
```powershell
.\Scripts\serial-monitor.ps1 COM15
```

**Terminal 2 - Send Commands:**
```powershell
.\Scripts\send-command.ps1 COM15 PNG
```

**Note:** You can't use the same COM port in two programs simultaneously. Use two different terminals or close the monitor before sending commands.

## Quick Reference

| Task | Command |
|------|---------|
| List COM ports | `.\Scripts\list-com-ports.ps1` |
| Monitor serial | `.\Scripts\serial-monitor.ps1 COM15` |
| Send command | `.\Scripts\send-command.ps1 COM15 PNG` |
| Full diagnosis | `.\Scripts\diagnose-uart-complete.ps1 COM15` |

## Example Session

```powershell
# 1. Find your COM port
PS> .\Scripts\list-com-ports.ps1
COM15 - USB Serial Port

# 2. Start monitoring
PS> .\Scripts\serial-monitor.ps1 COM15
Connected to COM15
Baud: 115200
Press Ctrl+C to exit

[14:30:15] OK STARTUP V2
[14:30:15] UART INIT OK
[14:30:15] OK DW3000_READY
[14:30:15] OK MAIN_LOOP

# 3. In another terminal, send command
PS> .\Scripts\send-command.ps1 COM15 PNG
✓ Response: OK
```
