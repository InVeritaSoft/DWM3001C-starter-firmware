# RS-485 Pin Connection Checklist

## Required Pin Connections

### ✅ Confirmed: TX Pin
- **Pin 8 on J10** = **GPIO 14 (P0.14)** = **UART TX** ✅
- Connect to RS-485 converter **DI (Data In)** pin

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
  
Pin 6 (J10)              GND                     Ground
  GND                    (Ground)
```

## Quick Test

**Watch the LEDs when you send a command:**

1. Send `PNG` command via Putty
2. **Orange LED blinks** = Command received ✅
3. **Green LED should blink** = Response being sent

**If green LED blinks but Putty shows nothing:**
- RS-485 bus wiring issue
- Converter configuration issue
- Baud rate mismatch

**If green LED doesn't blink:**
- `send_response()` not being called
- Command parsing issue
- Firmware problem

