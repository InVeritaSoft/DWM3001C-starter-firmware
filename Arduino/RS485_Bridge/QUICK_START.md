# Quick Start Guide - Arduino RS485 Bridge

## What Was Created

This implementation provides a complete Arduino Uno firmware that bridges RS485 communication to DWM3001CDK UWB modules.

### Files Created

1. **RS485_Bridge.ino** - Main Arduino firmware (ready to upload)
2. **README.md** - Complete documentation with wiring diagrams and troubleshooting
3. **DWM3001CDK_MODIFICATIONS.md** - Instructions for modifying DWM3001CDK firmware
4. **QUICK_START.md** - This file

## 5-Minute Setup

### Step 1: Wire the Hardware (5 minutes)

**MAX485 to Arduino:**
```
MAX485      →  Arduino Uno
VCC         →  5V
GND         →  GND
DI          →  D11
RO          →  D10
DE          →  D3
RE          →  D2
A           →  CAT5 Orange wire
B           →  CAT5 White-Orange wire
```

**Arduino to DWM3001CDK:**
```
Arduino Uno  →  DWM3001CDK
D9           →  GPIO15 (J10 Pin 10, RXD0)
D8           →  GPIO14 (J10 Pin 8, TXD0)
GND          →  GND
```

**Optional LEDs:**
```
Arduino Pin  →  LED Connection
D4           →  LED + 220Ω resistor to GND (RX indicator)
D5           →  LED + 220Ω resistor to GND (TX indicator)
D6           →  LED + 220Ω resistor to GND (Error indicator)
D13          →  Built-in LED (Heartbeat)
```

### Step 2: Upload Firmware (2 minutes)

1. Open `RS485_Bridge.ino` in Arduino IDE
2. Select **Tools → Board → Arduino Uno**
3. Select **Tools → Port → [Your Arduino's COM port]**
4. Click **Upload** button
5. Wait for "Done uploading" message

### Step 3: Verify Operation (1 minute)

After upload, watch the LEDs:

**Success:**
- All LEDs blink once on startup
- Heartbeat LED blinks 3 times quickly
- Heartbeat LED continues blinking at 1Hz

**Failure:**
- Error LED blinks rapidly then stays ON
- This means DWM3001CDK is not responding
- Check wiring and DWM3001CDK power

### Step 4: Test Communication (2 minutes)

Send commands via RS485:

```bash
# Ping test
echo "PNG" > /dev/ttyUSB0   # Linux/Mac
echo PNG > COM3             # Windows

# Expected response: OK

# Query node type
echo "NT" > /dev/ttyUSB0

# Expected response: OK TX_V2 or OK RX_V2
```

## Command Reference

| Command | Response | Description |
|---------|----------|-------------|
| PNG     | OK       | Check connectivity |
| NT      | OK TX_V2 or OK RX_V2 | Get node type |
| STRT    | OK START | Start UWB test |
| STOP    | OK STOP  | Stop UWB test |
| STAT    | OK STATS ... | Get statistics |

## Troubleshooting

### Error LED Stays ON
- **Problem:** DWM3001CDK not responding
- **Fix:** Check D8↔GPIO14 and D9↔GPIO15 wiring
- **Fix:** Ensure DWM3001CDK is powered and has firmware loaded

### No Response to Commands
- **Problem:** RS485 not working
- **Fix:** Swap A+ and B- wires
- **Fix:** Check MAX485 power (VCC and GND)

### Intermittent Errors
- **Problem:** SoftwareSerial at 115200 baud is unreliable
- **Fix:** Reduce baud rate to 57600 in firmware:
  ```cpp
  #define BAUD_RATE 57600  // Change from 115200
  ```
- **Fix:** Use shorter wires between Arduino and DWM3001CDK

## Architecture

```
┌─────────────────┐
│  Orchestrator   │  (PC or Raspberry Pi 5)
│   (PC/RPi5)     │
└────────┬────────┘
         │ USB
         ↓
┌─────────────────┐
│  USB-RS485      │
│    Adapter      │
└────────┬────────┘
         │ CAT5/CAT6 (A+, B-)
         │ Orange = A+
         │ White-Orange = B-
         ↓
┌─────────────────┐
│    MAX485       │  RS485 to TTL Converter
│   Converter     │  VCC=5V, GND, DI, RO, DE, RE, A, B
└────────┬────────┘
         │ TTL Serial
         │ D2, D3, D10, D11
         ↓
┌─────────────────┐
│  Arduino Uno    │  Transparent Bridge
│                 │  - Handles RS485 DE/RE control
│                 │  - Forwards commands bidirectionally
│                 │  - LED status indicators
└────────┬────────┘
         │ TTL Serial
         │ D8 (RX), D9 (TX)
         ↓
┌─────────────────┐
│  DWM3001CDK     │  UWB Module
│                 │  GPIO14 (TX), GPIO15 (RX)
│                 │  - TX or RX firmware
└────────┬────────┘
         │ UWB Radio
         ↓
   UWB Communication
```

## LED Indicators

| LED | Pin | Meaning |
|-----|-----|---------|
| Heartbeat | D13 | Blinks at 1Hz - Arduino is alive |
| RX Activity | D4 | Blinks when command received from orchestrator |
| TX Activity | D5 | Blinks when response sent to orchestrator |
| Error | D6 | Solid ON = communication error |

## Next Steps

1. **For TX Node:**
   - Upload Arduino firmware
   - Flash DWM3001CDK with `orchestrator_tx_v2.c`
   - Send STRT command to begin transmission

2. **For RX Node:**
   - Upload Arduino firmware (same firmware)
   - Flash DWM3001CDK with `orchestrator_rx_v2.c`
   - Send STRT command to begin reception

3. **Test End-to-End:**
   - Start both nodes with STRT command
   - Check statistics with STAT command
   - Verify UWB packets are being sent/received
   - Stop with STOP command

## DWM3001CDK Firmware Notes

The existing `orchestrator_tx_v2.c` and `orchestrator_rx_v2.c` firmware will work with minor modifications:

**Option 1 (Recommended):** Remove RS485_DE_PIN code blocks
- See `DWM3001CDK_MODIFICATIONS.md` for detailed instructions

**Option 2 (Easier):** Ensure RS485_DE_PIN is not defined
- Don't define RS485_DE_PIN in custom_board.h
- Code will automatically skip DE/RE control

## Performance

- **Baud Rate:** 115200 bps (both RS485 and DWM communication)
- **Command Latency:** ~10-50ms typical
- **Max RS485 Cable:** 50m (CAT5/CAT6)
- **Max Commands/sec:** ~100 (with fast responses)

## Support

Full documentation available in:
- **README.md** - Complete wiring and troubleshooting guide
- **DWM3001CDK_MODIFICATIONS.md** - DWM firmware modification guide

## Summary

✅ Arduino acts as transparent bridge between RS485 and DWM3001CDK  
✅ Automatic handshake on startup (NT command)  
✅ LED indicators for status monitoring  
✅ Error handling with clear error messages  
✅ Same firmware works for both TX and RX nodes  
✅ No command buffering - orchestrator must wait for responses  

**You're ready to test your UWB setup!** 🚀
