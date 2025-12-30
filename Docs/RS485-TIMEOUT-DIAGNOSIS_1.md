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
- [ ] **RO (Receive Out)** from converter → **GPIO 15** on board (RX pin)
- [ ] **DI (Data In)** to converter ← **GPIO 14** on board (TX pin)
- [ ] **GND** from converter → **GND** on board (Pin 6)
- [ ] Converter is powered (usually 5V or 3.3V)

**Common Mistakes:**
- ❌ RO connected to GPIO 14 (wrong - that's TX pin)
- ❌ DI connected to GPIO 15 (wrong - that's RX pin)

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
4. **If Green LED doesn't blink**: Board is NOT sending - check wiring or firmware

#### Step C: Test Direct Serial Connection
1. Connect serial terminal directly to board USB port (bypass RS-485)
2. Send `PNG\r\n`
3. Should receive `OK\r\n`
4. **If this works**: UART is fine, problem is RS-485 wiring/converter
5. **If this doesn't work**: Firmware issue or wrong firmware flashed

### 6. Common Issues and Solutions

#### Issue: Converter RX LED flashes but board doesn't respond
**Possible causes:**
1. **RO pin not connected correctly**
   - Check: RO → GPIO 15 (RX pin)
   - Use multimeter to verify continuity

2. **Converter not powered**
   - Check: Converter has power LED or measure voltage
   - Most converters need 5V or 3.3V

3. **Wrong converter type**
   - Some converters need auto-direction switching
   - Check converter datasheet for requirements

#### Issue: Board Orange LED blinks but no response
**Possible causes:**
1. **DI pin not connected**
   - Check: DI ← GPIO 14 (TX pin)
   - Response goes out this pin

2. **RS-485 bus termination**
   - Check: 120Ω resistor between A+ and B- at each end
   - Without termination, signals may not reach Pi5

3. **Converter direction switching**
   - Some converters need auto-direction switching
   - Check converter datasheet

#### Issue: No LEDs blink at all
**Possible causes:**
1. **Wrong firmware flashed**
   - Check: Is orchestrator v2 flashed?
   - Rebuild and flash: `make build && make flash`

2. **UART pins wrong**
   - Check: GPIO 15 = RX, GPIO 14 = TX
   - Verify in `custom_board.h`

3. **Board not powered**
   - Check: USB connected and board powered on

### 7. Advanced Diagnostics

#### Test UART Pins Directly
1. Use oscilloscope or logic analyzer
2. GPIO 15 (RX): Should see data when Pi5 sends
3. GPIO 14 (TX): Should see data when board responds
4. **If no data on GPIO 15**: RO pin not connected or converter issue
5. **If no data on GPIO 14**: Board not sending or DI pin not connected

### 8. Quick Fixes to Try

1. **Swap RO and DI connections** (if currently reversed)
   - RO → GPIO 15 (RX)
   - DI ← GPIO 14 (TX)

2. **Try different converter**
   - Some converters have different pinouts
   - Verify converter datasheet

3. **Check baud rate**
   - Must be exactly 115200
   - Check converter and Pi5 settings

4. **Check converter auto-direction**
   - Some converters automatically switch direction
   - Verify converter datasheet for requirements

### 9. What to Report

If still not working, provide:
1. Converter model/part number
2. Converter pinout diagram
3. Which LEDs blink (if any)
4. Results of direct serial test (USB port)
5. Photos of wiring if possible

