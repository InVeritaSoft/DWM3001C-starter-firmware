# RS-485 Automatic Direction Test

## Your Situation

You have an **RS485 to TTL 2126L** module that doesn't have visible DE/RE pins. This could mean:
1. DE/RE pins are internally tied (automatic direction)
2. DE/RE pins exist but are on the back or unlabeled
3. Different variant of the module

## Changes Made

I've updated the firmware to make GPIO 13 (DE/RE control) **optional**:

- If `RS485_DE_PIN` is defined → Uses GPIO 13 for direction control
- If `RS485_DE_PIN` is commented out → Assumes automatic direction control

## Current Configuration

The firmware is now configured for **automatic direction** (GPIO 13 disabled).

### File: `Src/custom_board.h`
```c
// RS485_DE_PIN is COMMENTED OUT (disabled)
// #define RS485_DE_PIN NRF_GPIO_PIN_MAP(0, 13)
```

## Test This Configuration

### Step 1: Flash Updated Firmware

```bash
cd ~/projects/DWM3001C-starter-firmware

# Flash Node A (TX)
./build-and-flash-tx.sh <JLINK_SERIAL_NODE_A>

# Flash Node B (RX)
./build-and-flash-rx.sh <JLINK_SERIAL_NODE_B>
```

### Step 2: Test with Orchestrator

```bash
cd Orchestrator
npm run web
```

### Step 3: Check Results

**If it works (PING succeeds):**
```
✅ Your module has automatic direction control
✅ No GPIO 13 connection needed
✅ Current configuration is correct
```

**If it still fails (PING timeout):**
```
❌ Your module needs manual direction control
❌ Need to find and connect DE/RE pins
❌ Or check wiring/configuration
```

## If Test Fails - Next Steps

### Option 1: Find the DE/RE Pins

Your 2126L module likely has 8 pins. Check:
- **Front side**: Look for pins labeled DE, RE, or EN
- **Back side**: Check for solder pads or through-holes
- **Datasheet**: Search "RS485 to TTL 2126L pinout"

### Option 2: Check Your Wiring

Verify these connections:
```
DWM3001C          2126L Module
---------         ------------
GPIO 14 (TX)  --> TXD (or DI)   ← Sends data TO module
GPIO 15 (RX)  <-- RXD (or RO)   ← Receives data FROM module
GND           --> GND
3.3V          --> VCC
```

**Common mistake:** TX/RX swapped
- DWM TX should go to module TXD (not RXD!)
- DWM RX should come from module RXD (not TXD!)

### Option 3: Try Swapping TX/RX

If wiring looks correct but still fails, try swapping:

Edit `Src/custom_board.h`:
```c
// Current (line 104-105):
#define RX_PIN_NUMBER  14
#define TX_PIN_NUMBER  15

// Try swapping:
#define RX_PIN_NUMBER  15
#define TX_PIN_NUMBER  14
```

Then rebuild and flash.

### Option 4: Enable GPIO 13 Control

If you find DE/RE pins or want to try with GPIO 13:

Edit `Src/custom_board.h`:
```c
// Uncomment this line:
#define RS485_DE_PIN NRF_GPIO_PIN_MAP(0, 13)
```

Then:
1. Connect GPIO 13 to DE/RE pins
2. Rebuild and flash
3. Test again

## Diagnostic Commands

### Test 1: Check if Firmware is Running
```bash
JLinkRTTClient
```

Should see:
```
=== FIRMWARE STARTING ===
ORCHESTRATOR TX v2.0
OK STARTUP V2
OK DW3000_READY
OK MAIN_LOOP
[DBG] RS-485 DE pin not defined - assuming automatic direction control
```

### Test 2: Direct Serial Test
```bash
# Send PING command
echo -e "PNG\r\n" > /dev/ttyUSB0

# Check for response (in another terminal)
timeout 2 cat /dev/ttyUSB0
```

Should see: `OK`

### Test 3: Check Module Pins

Count the pins on your 2126L module:
- **4 pins**: Minimal (VCC, GND, A, B) - automatic direction
- **6 pins**: Medium (VCC, GND, TXD, RXD, A, B) - might be automatic
- **8 pins**: Full (VCC, GND, TXD, RXD, DE, RE, A, B) - manual control needed

## Summary

**Current status:** Firmware configured for automatic direction (no GPIO 13)

**Next action:** Flash and test

**If it works:** ✅ Done! Your module is automatic
**If it fails:** Need to investigate wiring or find DE/RE pins

Let me know the test results and we'll proceed accordingly!
