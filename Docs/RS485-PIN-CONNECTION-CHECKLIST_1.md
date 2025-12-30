# RS-485 Pin Connection Checklist

## Required Pin Connections

### ✅ Confirmed: TX Pin
- **Pin 8 on J10** = **GPIO 14 (P0.14)** = **UART TX** ✅
- Connect to RS-485 converter **DI (Data In)** pin

### ⚠️ CRITICAL: DE/RE Direction Control Pin
- **GPIO P0.06** = **RS485_DE_RE_PIN** ⚠️
- **This is NOT on J10 connector!**
- Connect to RS-485 converter **DE/RE** pin
- **Without this, responses can't be sent!**

### ✅ RX Pin
- **Pin 10 on J10** = **GPIO 15 (P0.15)** = **UART RX**
- Connect to RS-485 converter **RO (Receive Out)** pin

## Complete Connection Map

```
DWM3001CDK Board          RS-485 Converter        Function
─────────────────        ──────────────────      ──────────
Pin 8 (J10)              DI (Data In)            TX → Converter
  GPIO 14 (P0.14)        (TTL input)
  
Pin 10 (J10)             RO (Receive Out)        RX ← Converter  
  GPIO 15 (P0.15)        (TTL output)
  
P0.06 ⚠️                  DE/RE                   Direction Control
  (NOT on J10!)          (Direction Enable)       CRITICAL!
  
Pin 6 (J10)              GND                     Ground
  GND                    (Ground)
```

## The Problem: DE/RE Pin Location

**⚠️ IMPORTANT:** GPIO P0.06 is **NOT on the J10 connector!**

You need to find where P0.06 is accessible on your board. It might be:
- On a different connector
- On a test point
- On a breakout header
- Need to add a wire connection

## How to Find P0.06

### Option 1: Check Board Schematics
Look for GPIO P0.06 or pin 6 of port 0 in the DWM3001CDK schematics.

### Option 2: Check if Available Elsewhere
- Look for other connectors on the board
- Check if there's a GPIO breakout header
- Look for test points labeled P0.06 or GPIO6

### Option 3: Use Multimeter
1. Power on board
2. Set multimeter to continuity mode
3. Probe different pins/connectors
4. Look for pin that goes HIGH when sending response (if firmware is running)

## Why This Matters

**Without DE/RE pin connected:**
- ✅ Commands received (RX works) → Orange LED blinks
- ❌ Responses can't be sent (TX fails) → Putty shows nothing

**With DE/RE pin connected:**
- ✅ Commands received → Orange LED blinks
- ✅ Responses sent → Green LED blinks → Putty shows "OK"

## Quick Test

**Watch the Green LED when you send a command:**

1. Send `PNG` command via Putty
2. **Orange LED blinks** = Command received ✅
3. **Green LED should blink** = Response being sent

**If green LED blinks but Putty shows nothing:**
- DE/RE pin not connected (most likely)
- Or DE/RE connected but RS-485 bus wiring issue

**If green LED doesn't blink:**
- `send_response()` not being called
- Command parsing issue
- Firmware problem

## Next Steps

1. **Find where P0.06 is accessible on your board**
2. **Connect DE/RE pin from RS-485 converter to P0.06**
3. **Test again** - Putty should now show responses

## Alternative: Check if P0.06 is Available

If P0.06 is not accessible, you might need to:
- Use a different GPIO pin (requires code change)
- Add a breakout connection
- Check if your board has GPIO pins available elsewhere

Let me know if you can find P0.06 on your board, or if you need help finding an alternative GPIO pin!

