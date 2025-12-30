# RS-485 Direction Control Fix

## Problem
The firmware was sending correct bytes but responses weren't reaching the PC because the RS-485 transceiver wasn't switching direction properly. RS-485 is half-duplex and requires direction control.

## Solution
Added GPIO pin control for RS-485 transceiver direction (DE/RE pin).

## Changes Made

### 1. Pin Definition (`Src/custom_board.h`)
- Added `RS485_DE_RE_PIN` definition: GPIO P0.06
- This pin controls the RS-485 transceiver direction

### 2. Firmware Updates (Both TX and RX v2)
- **Initialization**: Configure GPIO pin as output, start in RX mode (LOW)
- **Before sending response**: Set pin HIGH (TX mode) to enable transmission
- **After sending response**: Set pin LOW (RX mode) to enable reception

## Hardware Connection

**IMPORTANT**: Connect the RS-485 transceiver's DE/RE pin to **GPIO P0.06** on the DWM3001CDK board.

### Pin Mapping:
```
DWM3001CDK Board (J10)    RS-485 Transceiver
─────────────────────    ───────────────────
Pin 8 (P0.08/TXD)    →   DI (Data In)
Pin 10 (P0.10/RXD)   →   RO (Receive Out)
Pin 6 (GND)          →   GND
P0.06 (NEW!)         →   DE/RE (Direction Enable)
```

### RS-485 Bus Connection:
```
RS-485 Transceiver    RS-485 Bus
──────────────────    ───────────
A+                →   A+ (differential positive)
B-                →   B- (differential negative)
```

## How It Works

1. **Default State (RX Mode)**:
   - DE/RE pin = LOW (0)
   - Transceiver receives data from RS-485 bus
   - Firmware can receive commands from PC

2. **When Sending Response (TX Mode)**:
   - DE/RE pin = HIGH (1) 
   - Transceiver drives the RS-485 bus
   - Firmware sends response to PC
   - After transmission completes, pin returns to LOW

3. **Timing**:
   - Pin set HIGH before sending
   - 2ms delay after sending to ensure transmission completes
   - Pin set LOW to return to RX mode

## Testing

After flashing the updated firmware:

1. **Connect RS-485 transceiver DE/RE pin to P0.06**
2. **Power cycle the board**
3. **Test with serial terminal**:
   ```powershell
   # Should see startup messages:
   OK STARTUP V2
   OK DW3000_READY
   OK MAIN_LOOP
   ```
4. **Send PING command**:
   ```
   PNG
   ```
   Should receive: `OK`

5. **Watch LEDs**:
   - D10 (Orange) should blink when command received
   - D11 (Green) should blink when response sent

## Verification

If working correctly:
- ✅ Startup messages appear in serial terminal
- ✅ Commands receive responses
- ✅ D10 (Orange) LED blinks on command reception
- ✅ D11 (Green) LED blinks on response transmission
- ✅ No communication timeouts

## Notes

- The pin is GPIO P0.06, which is available on the DWM3001CDK board
- If your RS-485 transceiver uses separate DE and RE pins, connect both to P0.06
- Some transceivers have auto-direction switching - this fix is for manual control transceivers
- The 1ms delays ensure the transceiver has time to switch modes

