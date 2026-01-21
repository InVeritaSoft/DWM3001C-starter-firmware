# Arduino→DWM Communication Debug Guide

## Current Status

✅ **Firmware compiled** with debug instrumentation  
✅ **Test mode implemented** - send 'T' via USB Serial  
✅ **Debug logs added** to track Arduino→DWM communication  
❌ **Serial output capture** - requires manual monitoring via Arduino IDE

## How to Debug

### Step 1: Open Arduino IDE Serial Monitor

1. Open Arduino IDE
2. Tools → Serial Monitor
3. Set baud rate to **115200**
4. Select the Arduino port (e.g., `/dev/cu.usbmodem0007602011681`)

### Step 2: Enable Test Mode

In the Serial Monitor, type `T` and press Enter. This will:
- Enable automatic test mode
- Send test commands (PNG, NODE_TYPE, STAT) every 5 seconds
- Show detailed debug output

### Step 3: Look for These Debug Messages

#### Startup Messages (should appear immediately):
```
=== RS485 Bridge Firmware Starting ===
[SETUP] LED pins initialized
[SETUP] RS485 control pins initialized
[SETUP] RS485 serial initialized at 57600 baud, DWM serial at 115200 baud
```

#### DWM Communication Check:
```
[SETUP] DWM3001CDK Communication Check
[SETUP] Checking for DWM3001CDK startup messages...
[SETUP] Listening for 3 seconds...
```

**Expected:** Should see startup messages from DWM (e.g., "OK STARTUP", "OK FIRMWARE_RUNNING")

#### Handshake:
```
[SETUP] Starting handshake with DWM3001CDK...
[HANDSHAKE] Attempt 1/3: Sending NODE_TYPE command...
```

**Expected:** Should see "OK NODE_TYPE=TX_V2" or "OK NODE_TYPE=RX_V2"

#### Test Mode (after sending 'T'):
```
[TEST] Sending test command #1: "PNG"
[DEBUG] dwm_send_command ENTRY: command="PNG", len=3, DWMSerial.isListening()=YES
[DEBUG] BEFORE send: command="PNG"
[DEBUG] Sending bytes: 0x50 0x4E 0x47 0x0D 0x0A (\r\n)
[DEBUG] AFTER send: command sent to DWMSerial
[DEBUG] dwm_receive_response ENTRY
[DEBUG] Initial bytes available: 0
[DWM RX] Waiting for response (timeout=5000ms)...
```

**Expected:** Should see:
- `[DEBUG] FIRST BYTE received: 0x4F` (for 'O' in "OK")
- `[DEBUG] RESPONSE COMPLETE: "OK", len=2`
- `[TEST] ✅ SUCCESS - Response: "OK"`

## Common Issues and What to Look For

### Issue 1: No DWM Startup Messages
**Symptoms:**
```
[SETUP] ❌ WARNING: No startup messages received from DWM3001CDK
```

**Possible causes:**
- DWM not powered
- Wiring incorrect (GPIO14→D9, GPIO15→D8, GND→GND)
- DWM firmware not running
- Baud rate mismatch

### Issue 2: Handshake Fails
**Symptoms:**
```
[HANDSHAKE] No response received
[SETUP] Handshake FAILED - Error LED active
```

**Look for:**
- `[DEBUG] Initial bytes available: X` - if > 0, data is coming but not parsed correctly
- `[DEBUG] TIMEOUT: elapsed=5000ms, bytesReceived=0` - DWM not responding
- `[DEBUG] FIRST BYTE received: 0xXX` - if present, DWM is responding but format might be wrong

### Issue 3: Test Commands Timeout
**Symptoms:**
```
[TEST] ❌ FAILED - No response received
[DEBUG] TIMEOUT: elapsed=5000ms, bytesReceived=0
```

**Possible causes:**
- DWM serial not listening (check `DWMSerial.isListening()`)
- Command not reaching DWM (check wiring)
- DWM in wrong state
- Response format mismatch

## What to Share for Analysis

When reporting issues, please share:

1. **Startup sequence** - All messages from "=== RS485 Bridge Firmware Starting ==="
2. **DWM communication check** - What appears during the 3-second startup check
3. **Handshake attempt** - Full output of handshake attempts
4. **Test mode output** - At least 2-3 test command cycles
5. **Any error messages** - Especially `[DEBUG]` and `[ERROR]` lines

## Expected Successful Output

```
=== RS485 Bridge Firmware Starting ===
[SETUP] LED pins initialized
...
[SETUP] ✅ Received X bytes from DWM3001CDK
[SETUP] ✅ DWM3001CDK firmware is running!
[HANDSHAKE] Success! Node type: TX_V2
[SETUP] Handshake SUCCESS - Node Type: TX_V2
[TEST] Test mode ENABLED
[TEST] Sending test command #1: "PNG"
[DEBUG] dwm_send_command ENTRY: command="PNG"...
[DEBUG] FIRST BYTE received: 0x4F
[DEBUG] RESPONSE COMPLETE: "OK", len=2
[TEST] ✅ SUCCESS - Response: "OK"
```

## Next Steps

1. Open Serial Monitor in Arduino IDE
2. Send 'T' to enable test mode
3. Copy the serial output
4. Share it for analysis - the debug instrumentation will show exactly where the communication is failing
