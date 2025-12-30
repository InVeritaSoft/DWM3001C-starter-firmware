# RS-485 Timeout Diagnosis Guide

## Quick Checklist

### 1. Verify Firmware is Running
- [ ] Power cycle the board
- [ ] Check if Blue LED (LED 3) turns on (indicates UART initialized)
- [ ] Check if Red LED blinks twice on startup (firmware started)
- [ ] Connect serial terminal directly to board USB port (not RS-485)
  - Should see: `OK STARTUP V2`, `OK DW3000_READY`, `OK MAIN_LOOP`
  - If you see these messages, UART TX is working!

### 2. Verify RS-485 Converter Wiring

**Critical Connections:**
- [ ] **RO (Receive Out)** from converter → **GPIO 19** on board (RX pin)
- [ ] **DI (Data In)** to converter ← **GPIO 15** on board (TX pin)
- [ ] **GND** from converter → **GND** on board (Pin 6)
- [ ] **DE/RE** from converter → **P0.06** on board (direction control)
- [ ] Converter is powered (usually 5V or 3.3V)

**Common Mistakes:**
- ❌ RO connected to GPIO 15 (wrong - that's TX pin)
- ❌ DI connected to GPIO 19 (wrong - that's RX pin)
- ❌ DE/RE not connected (firmware won't switch direction)

### 3. Check RS-485 Converter Type

**Does your converter have:**
- [ ] **Manual DE/RE control** (requires P0.06 connection) ← **You need this**
- [ ] **Auto-direction switching** (may not work with firmware)

**If your converter has auto-direction:**
- The firmware expects manual control via P0.06
- Auto-direction converters may not work correctly
- Try connecting DE/RE to P0.06 anyway

### 4. Test Data Flow

**What you're seeing:**
- ✅ Pi5 sends → USB RS-485 adapter TX LED flashes
- ✅ RS-485 converter RX LED flashes (receiving from bus)
- ❌ Board doesn't respond (timeout)

**This suggests:**
- Data is reaching the converter from RS-485 bus ✓
- Data may NOT be reaching the board from converter ✗

### 5. Diagnostic Steps

#### Step A: Check if Board Receives Data
1. Power cycle board
2. Watch Orange LED (LED 1) - should blink when command received
3. Send PING command via RS-485
4. **If Orange LED blinks**: Board is receiving! Problem is response not getting back
5. **If Orange LED doesn't blink**: Board is NOT receiving - check wiring

#### Step B: Check if Board Sends Responses
1. Send PING command
2. Watch Green LED (LED 2) - should blink when response sent
3. **If Green LED blinks**: Board is sending! Problem is response not reaching Pi5
4. **If Green LED doesn't blink**: Board is NOT sending - check DE/RE pin

#### Step C: Test Direct Serial Connection
1. Connect serial terminal directly to board USB port (bypass RS-485)
2. Send `PNG\r\n`
3. Should receive `OK\r\n`
4. **If this works**: UART is fine, problem is RS-485 wiring/converter
5. **If this doesn't work**: Firmware issue or wrong firmware flashed

### 6. Common Issues and Solutions

#### Issue: Converter RX LED flashes but board doesn't respond
**Possible causes:**
1. **RO pin not connected to GPIO 19**
   - Check: RO → GPIO 19 (not GPIO 15!)
   - Use multimeter to verify continuity

2. **DE/RE pin not connected**
   - Check: DE/RE → P0.06
   - Without this, converter stays in wrong direction

3. **Converter not powered**
   - Check: Converter has power LED or measure voltage
   - Most converters need 5V or 3.3V

4. **Wrong converter type**
   - Some converters need different DE/RE logic
   - Try inverting DE/RE (connect to GND instead of P0.06)

#### Issue: Board Orange LED blinks but no response
**Possible causes:**
1. **DE/RE pin not switching**
   - Check: DE/RE connected to P0.06
   - Firmware sets it HIGH before sending, LOW after

2. **DI pin not connected**
   - Check: DI ← GPIO 15
   - Response goes out this pin

3. **RS-485 bus termination**
   - Check: 120Ω resistor between A+ and B- at each end
   - Without termination, signals may not reach Pi5

#### Issue: No LEDs blink at all
**Possible causes:**
1. **Wrong firmware flashed**
   - Check: Is orchestrator v2 flashed?
   - Rebuild and flash: `make build && make flash`

2. **UART pins wrong**
   - Check: GPIO 19 = RX, GPIO 15 = TX (after swap)
   - Verify in `custom_board.h`

3. **Board not powered**
   - Check: USB connected and board powered on

### 7. Advanced Diagnostics

#### Test DE/RE Pin Manually
1. Connect multimeter to P0.06
2. Power on board
3. Should read LOW (0V) - RX mode
4. Send command that triggers response
5. Should see pin go HIGH briefly during response
6. **If pin doesn't change**: DE/RE not connected or firmware issue

#### Test UART Pins Directly
1. Use oscilloscope or logic analyzer
2. GPIO 19 (RX): Should see data when Pi5 sends
3. GPIO 15 (TX): Should see data when board responds
4. **If no data on GPIO 19**: RO pin not connected or converter issue
5. **If no data on GPIO 15**: Board not sending or DI pin not connected

### 8. Quick Fixes to Try

1. **Swap RO and DI connections** (if currently reversed)
   - RO → GPIO 19
   - DI ← GPIO 15

2. **Check DE/RE connection**
   - Ensure DE/RE → P0.06
   - If converter has separate DE and RE, connect both to P0.06

3. **Try different converter**
   - Some converters have different pinouts
   - Verify converter datasheet

4. **Check baud rate**
   - Must be exactly 115200
   - Check converter and Pi5 settings

5. **Add pull-up/pull-down resistors**
   - Some converters need pull-up on DE/RE
   - Try 10kΩ pull-up to 3.3V on DE/RE

### 9. What to Report

If still not working, provide:
1. Converter model/part number
2. Converter pinout diagram
3. Which LEDs blink (if any)
4. Results of direct serial test (USB port)
5. Multimeter readings on P0.06 (DE/RE pin)
6. Photos of wiring if possible

