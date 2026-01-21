# RS485 Bridge Power & Voltage Checklist

## ⚠️ CRITICAL: Power Connections

If commands aren't reaching the Arduino (no LED activity, no serial data), check these power connections:

### 1. MAX485 Module Power (MOST COMMON ISSUE)

**Required Connections:**
```
MAX485 Module:
  VCC  → Arduino 5V pin (NOT 3.3V!)
  GND  → Arduino GND pin
```

**Check:**
- ✅ MAX485 VCC pin connected to Arduino 5V pin
- ✅ MAX485 GND pin connected to Arduino GND pin
- ✅ MAX485 module has power LED lit (if it has one)
- ✅ Measure voltage between MAX485 VCC and GND = **5.0V** (not 3.3V)

**Common Mistakes:**
- ❌ Connected VCC to 3.3V instead of 5V
- ❌ Forgot to connect GND
- ❌ Loose connection on VCC or GND
- ❌ MAX485 module not powered at all

### 2. Arduino Power

**Required:**
- ✅ Arduino powered via USB or external power supply
- ✅ Arduino power LED (usually near USB connector) is ON
- ✅ Arduino 5V pin outputs 5.0V (measure with multimeter)
- ✅ Arduino GND is common ground for all components

**Check:**
```bash
# Measure Arduino 5V pin voltage:
# Should read: 4.8V - 5.2V
# If lower: Arduino power supply insufficient
```

### 3. RS485 Bus Power & Termination

**Required:**
- ✅ RS485 A+ and B- lines connected
- ✅ 120Ω termination resistor at each end of RS485 bus (if long cable)
- ✅ Common GND between orchestrator and Arduino

**Check:**
```
Orchestrator (PC/USB-RS485 Adapter):
  A+  → CAT5 Orange wire
  B-  → CAT5 White-Orange wire
  GND → CAT5 Ground wire (if available)

Arduino/MAX485:
  A   → CAT5 Orange wire (A+)
  B   → CAT5 White-Orange wire (B-)
  GND → CAT5 Ground wire (if available)
```

### 4. DWM3001CDK Power

**Required:**
- ✅ DWM3001CDK powered separately (USB or external supply)
- ✅ DWM3001CDK power LED is ON
- ✅ Common GND between Arduino and DWM3001CDK

**⚠️ IMPORTANT:** Do NOT connect Arduino 5V to DWM3001CDK! DWM3001CDK has its own power supply.

### 5. Voltage Level Compatibility

**MAX485 Module Types:**

**5V MAX485 Modules:**
- ✅ VCC → Arduino 5V
- ✅ Works with Arduino Uno (5V logic)
- ✅ Most common type

**3.3V MAX485 Modules:**
- ⚠️ VCC → Arduino 3.3V (NOT 5V!)
- ⚠️ May not work reliably with Arduino Uno (5V logic levels)
- ⚠️ Better for 3.3V microcontrollers

**Check Your Module:**
1. Look for markings on MAX485 module: "5V" or "3.3V"
2. If unsure, check module documentation
3. If module gets hot with 5V, it's probably 3.3V module

## 🔍 Diagnostic Steps

### Step 1: Check MAX485 Power

1. **Visual Check:**
   - Is MAX485 module LED lit? (if it has one)
   - Are VCC and GND wires securely connected?

2. **Voltage Check:**
   ```
   Multimeter:
   - Red probe → MAX485 VCC pin
   - Black probe → MAX485 GND pin
   - Should read: 4.8V - 5.2V
   ```

3. **If No Voltage:**
   - Check Arduino 5V pin voltage
   - Check wire connections
   - Try different wires
   - Check for loose breadboard connections

### Step 2: Check Arduino Power

1. **Visual Check:**
   - Arduino power LED ON?
   - USB cable connected securely?

2. **Voltage Check:**
   ```
   Multimeter:
   - Red probe → Arduino 5V pin
   - Black probe → Arduino GND pin
   - Should read: 4.8V - 5.2V
   ```

### Step 3: Check RS485 Communication

1. **Open Arduino Serial Monitor** (USB, 115200 baud)
2. **Send command via RS485** from orchestrator
3. **Watch Serial Monitor** for:
   - `[RS485] Activity detected` - RS485 is receiving data
   - `[RS485] Received command` - Command parsed successfully
   - If nothing appears: MAX485 not powered or not connected

### Step 4: Test MAX485 Module

**Simple Test:**
1. Connect MAX485 VCC → Arduino 5V
2. Connect MAX485 GND → Arduino GND
3. Connect MAX485 DE → Arduino D3
4. Connect MAX485 RE → Arduino D2
5. Upload firmware
6. Check Serial Monitor for startup messages

**If Still No Response:**
- MAX485 module might be faulty
- Try different MAX485 module
- Check if module is "Automatic" type (needs different wiring)

## 🔧 "Automatic RS485 to TTL" vs MAX485

### Automatic RS485 to TTL Converter
- **Usually has:** Built-in direction control (no DE/RE pins needed)
- **Power:** Often 5V, sometimes 3.3V
- **Wiring:** Simpler (just RX, TX, VCC, GND)
- **Advantage:** No direction control needed

### MAX485 Module
- **Requires:** Manual direction control via DE/RE pins
- **Power:** Usually 5V
- **Wiring:** More complex (DI, RO, DE, RE, VCC, GND, A, B)
- **Advantage:** More control, lower cost

### If You Had "Automatic" Converter Working:

**Check what was different:**
1. Was it powered differently?
2. Did it have different voltage requirements?
3. Was it connected directly to DWM3001CDK (bypassing Arduino)?

**Current Setup (Arduino Bridge):**
- MAX485 needs manual DE/RE control
- Arduino handles direction switching
- More complex but more reliable

## 📋 Complete Power Checklist

```
[ ] Arduino powered (USB or external supply)
[ ] Arduino power LED ON
[ ] Arduino 5V pin = 5.0V (measured)
[ ] MAX485 VCC → Arduino 5V
[ ] MAX485 GND → Arduino GND
[ ] MAX485 VCC-GND voltage = 5.0V (measured)
[ ] MAX485 module LED ON (if present)
[ ] DWM3001CDK powered separately
[ ] DWM3001CDK power LED ON
[ ] Common GND: Arduino GND = MAX485 GND = DWM3001CDK GND
[ ] RS485 bus GND connected (if available)
[ ] All connections secure (no loose wires)
```

## 🚨 Quick Fixes

### No Response at All (No LED Activity)

1. **Check MAX485 VCC:**
   ```bash
   # Measure MAX485 VCC pin voltage
   # If 0V: Check Arduino 5V connection
   # If <4.5V: Arduino power supply issue
   # If 5V: MAX485 should be powered
   ```

2. **Check MAX485 GND:**
   ```bash
   # Measure continuity between MAX485 GND and Arduino GND
   # Should be 0Ω (short circuit)
   # If open circuit: GND not connected
   ```

3. **Try Different Power Source:**
   - Use external 5V power supply for MAX485
   - Connect: External 5V → MAX485 VCC, External GND → MAX485 GND
   - Keep GND common with Arduino

### Partial Response (LEDs Blink But No Data)

1. **Check RS485 Bus:**
   - A+ and B- lines connected correctly
   - No shorts between A+ and B-
   - Proper termination resistors (120Ω at each end)

2. **Check Direction Control:**
   - DE pin connected to Arduino D3
   - RE pin connected to Arduino D2
   - Both pins working (check with multimeter)

## 📞 Still Not Working?

If you've checked all power connections and still no response:

1. **Try Different MAX485 Module** (might be faulty)
2. **Check Arduino Serial Monitor** for error messages
3. **Verify Firmware Upload** (re-upload firmware)
4. **Test with Direct Connection** (bypass MAX485, connect directly to DWM3001CDK)

## 🔗 Related Documentation

- `README.md` - Complete wiring diagram
- `QUICK_START.md` - Quick setup guide
- `DWM3001CDK_MODIFICATIONS.md` - DWM3001CDK firmware setup
