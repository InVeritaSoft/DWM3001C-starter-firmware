# Troubleshooting: PNG Command No Response

## Quick Diagnostic

Run the diagnostic script:
```bash
python3 Zephyr/tests/diagnose_no_response.py /dev/ttyUSB0
```

This will check:
- Startup messages from firmware
- PNG command response
- Other commands (PING, NODE_TYPE, STAT)

## Most Likely Issues

### 1. Firmware Not Rebuilt/Reflashed

**If you modified orchestrator_v2 firmware:**
- You need to **rebuild and reflash** the firmware
- The changes won't take effect until you flash the new binary

**Steps:**
```bash
# Navigate to project root
cd /path/to/DWM3001C-starter-firmware

# Rebuild orchestrator_v2 firmware
# (Check your build system - might be Makefile, Segger, etc.)

# Flash the firmware
# (Use your flashing tool - J-Link, nrfjprog, etc.)
```

### 2. Wrong Firmware Flashed

**Check which firmware is running:**

**Orchestrator v2 firmware:**
- Startup message: "OK STARTUP V2"
- Uses GPIO 14/15 (P0.14/P0.15)
- Responds with "OK" (not "OK PONG")

**Zephyr firmware:**
- Startup message: "UWB Test Node Firmware Starting..."
- Uses GPIO 14/15 (after our fix) or GPIO 8/10 (before fix)
- Responds with "OK PONG"

**To check:**
1. Connect to serial port and look for startup messages
2. Or check which firmware you last flashed

### 3. Hardware Connection Issues

**Verify RS-485 connections:**
- J10 Pin 2 or 4 (5V0) → RS-485 Module VCC
- J10 Pin 6 (GND) → RS-485 Module GND
- J10 Pin 8 (TXD0) → RS-485 Module TXD
- J10 Pin 10 (RXD0) → RS-485 Module RXD

**Check:**
- Power LED on RS-485 module
- Continuity with multimeter
- No loose connections

### 4. Wrong Serial Port

**On Linux:**
- `/dev/ttyUSB0` = USB-to-RS485 adapter (for commands) ✅
- `/dev/ttyACM0` = J-Link CDC UART (for debug, NOT commands)

**Verify:**
```bash
ls -l /dev/ttyUSB*
dmesg | tail  # Check recent USB device connections
```

### 5. UART Pins Mismatch

**Orchestrator v2:** GPIO 14/15 ✅ (correct for J10 Pin 8/10)
**Zephyr (before fix):** GPIO 8/10 ❌ (wrong)
**Zephyr (after fix):** GPIO 14/15 ✅ (correct)

**If using Zephyr firmware:**
- Make sure you rebuilt with the fixed `dwm3001cdk.overlay`
- Verify the overlay has `tx-pin = <14>` and `rx-pin = <15>`

## Step-by-Step Troubleshooting

### Step 1: Check Startup Messages

```bash
# Connect to serial port and look for startup
python3 -c "
import serial, time
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=3.0)
time.sleep(2)
if ser.in_waiting > 0:
    print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'))
else:
    print('No startup messages')
ser.close()
"
```

**Expected:**
- Orchestrator v2: "OK STARTUP V2"
- Zephyr: "UWB Test Node Firmware Starting..."

**If no messages:**
- Firmware might not be running
- Wrong serial port
- Hardware connection issue

### Step 2: Test Different Commands

```bash
python3 Zephyr/tests/diagnose_no_response.py /dev/ttyUSB0
```

This tests:
- PNG
- PING
- NODE_TYPE
- STAT

### Step 3: Verify Firmware Changes

**If you modified orchestrator_v2:**
1. Check that changes are in source files
2. Rebuild firmware
3. Reflash firmware
4. Test again

**If using Zephyr:**
1. Check `Zephyr/dwm3001cdk.overlay` has `tx-pin = <14>` and `rx-pin = <15>`
2. Rebuild: `west build -b decawave_dwm3001cdk -- -DNODE_TYPE=A -t clean`
3. Flash: `west flash --runner jlink`
4. Test again

### Step 4: Check Hardware

1. **Power:**
   - RS-485 module LED should be on
   - Check voltage at J10 Pin 2/4 (should be 5V or 3.3V)

2. **Connections:**
   - Verify all 4 connections (VCC, GND, TX, RX)
   - Check for loose wires
   - Verify TX/RX aren't swapped

3. **RS-485 Module:**
   - Try a different RS-485 module
   - Check module specifications (baud rate support, etc.)

## Quick Fixes to Try

### Fix 1: Rebuild and Reflash

```bash
# For orchestrator_v2 (check your build system)
make clean
make
# Flash using your tool

# For Zephyr
cd Zephyr
west build -b decawave_dwm3001cdk -- -DNODE_TYPE=A -t clean
west build -b decawave_dwm3001cdk -- -DNODE_TYPE=A
west flash --runner jlink
```

### Fix 2: Try Different Serial Port

```bash
# List available ports
ls -l /dev/ttyUSB* /dev/ttyACM*

# Try different port
python3 Zephyr/tests/test_png_continuous.py /dev/ttyUSB1
```

### Fix 3: Check Baud Rate

Verify both sides use 115200:
- Firmware: 115200 (30801920 in nRF SDK)
- Test script: 115200

### Fix 4: Enable Console (Zephyr only)

If using Zephyr, temporarily enable console to see debug messages:

Edit `Zephyr/dwm3001cdk.overlay`:
```dts
/ {
	chosen {
		// Comment out to enable console
		// /delete-property/ zephyr,console;
		// /delete-property/ zephyr,shell-uart;
	};
};
```

Then connect to J-Link CDC UART port (`/dev/ttyACM0`) to see debug output.

## Still Not Working?

1. **Run full diagnostic:**
   ```bash
   python3 Zephyr/tests/diagnose_no_response.py /dev/ttyUSB0
   ```

2. **Check firmware source:**
   - Verify UART pins are GPIO 14/15
   - Verify baud rate is 115200
   - Check UART initialization code

3. **Test with different hardware:**
   - Try different RS-485 module
   - Try direct UART connection (bypass RS-485)

4. **Check for firmware crashes:**
   - Look for error LEDs
   - Check if firmware completes initialization
   - Verify UWB chip initialization doesn't block UART

## Expected Behavior When Working

**Startup:**
```
OK STARTUP V2
```

**PNG command:**
```
→ PNG
← OK
```

**Or for Zephyr:**
```
→ PNG
← OK PONG
```
