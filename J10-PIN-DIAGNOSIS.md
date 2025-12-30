# J10 Pin Diagnosis - USB Works But J10 Doesn't

## Problem Summary
- ✅ USB connection (Putty) works - firmware responds
- ✅ Blue LED blinks when typing PNG via USB
- ❌ J10 pins (GPIO 15/19) don't work via RS-485

## Critical Checks

### 1. Verify J10 Pin Numbers
**Question:** Which physical pins on J10 connector are GPIO 15 and GPIO 19?

The firmware uses:
- **GPIO 15 (P0.15)** = TX pin
- **GPIO 19 (P0.19)** = RX pin

**Action:** Check your board's J10 pinout diagram to find which physical pins correspond to P0.15 and P0.19.

### 2. Verify RS-485 Converter Wiring

**Current Configuration (After Pin Swap):**
```
RS-485 Converter          DWM3001CDK Board (J10)
─────────────────        ────────────────────────
RO (Receive Out)    →    GPIO 19 (P0.19) - RX pin
DI (Data In)        ←    GPIO 15 (P0.15) - TX pin
GND                 →    GND (Pin 6)
DE/RE               →    P0.06 (direction control)
```

**Check:**
- [ ] RO from converter → GPIO 19 on board
- [ ] DI to converter ← GPIO 15 on board  
- [ ] GND connected
- [ ] DE/RE connected to P0.06
- [ ] Converter is powered (check power LED if available)

### 3. Test with Multimeter

**Test GPIO 19 (RX pin):**
1. Set multimeter to continuity mode
2. One probe on RS-485 converter RO pin
3. Other probe on board GPIO 19 (P0.19)
4. Should show continuity (beep)

**Test GPIO 15 (TX pin):**
1. One probe on RS-485 converter DI pin
2. Other probe on board GPIO 15 (P0.15)
3. Should show continuity (beep)

**Test P0.06 (DE/RE):**
1. One probe on RS-485 converter DE/RE pin
2. Other probe on board P0.06
3. Should show continuity (beep)

### 4. Check RS-485 Converter Power

**Verify converter is powered:**
- [ ] Converter has power LED (should be on)
- [ ] Measure voltage on converter VCC pin (should be 3.3V or 5V)
- [ ] Check converter datasheet for power requirements

### 5. Test DE/RE Pin Behavior

**When board is powered:**
1. Measure voltage on P0.06 (DE/RE pin)
2. Should read LOW (0V) when idle (RX mode)
3. When board sends response, should briefly go HIGH (3.3V)

**If DE/RE doesn't change:**
- DE/RE pin not connected
- Firmware not controlling it
- Wrong pin connected

### 6. Check for Pin Conflicts

**GPIO 19 (P0.19) is also used for:**
- ARDUINO_9_PIN = DW3000_WKUP_Pin (wake-up pin for UWB chip)

**Check if there's a conflict:**
- Is GPIO 19 being used for something else?
- Is there a jumper or switch that routes GPIO 19 elsewhere?

### 7. Verify UART Peripheral

The firmware uses **UART0** with:
- RX pin: GPIO 19 (P0.19)
- TX pin: GPIO 15 (P0.15)

**Important:** Not all GPIO pins can be used for UART. The nRF52833 has specific UART pins:
- UART0 can use various pins (configurable)
- But GPIO 15 and 19 must be valid UART0 pins

**Check:** Verify GPIO 15 and 19 are valid UART0 pins on nRF52833.

### 8. Test Direct Connection (Bypass RS-485)

**Try connecting directly:**
1. Connect Pi5 TX → Board GPIO 19 (RX) directly (no RS-485 converter)
2. Connect Pi5 RX ← Board GPIO 15 (TX) directly
3. Connect GND
4. Test communication

**If this works:** Problem is RS-485 converter or wiring
**If this doesn't work:** Problem is J10 pins or firmware configuration

### 9. Check Board Documentation

**Look for:**
- J10 pinout diagram
- Which pins are GPIO 15 and GPIO 19
- Any jumpers or switches that affect these pins
- Any notes about UART pin configuration

### 10. Alternative: Use Different Pins

If GPIO 15/19 don't work, you might need to:
1. Check if board has different UART pins exposed on J10
2. Modify firmware to use different pins (if valid UART pins)
3. Check board schematics for actual UART pin locations

## Most Likely Issues

1. **Wrong physical pins on J10**
   - GPIO 15/19 might not be on the pins you think
   - Check board pinout diagram

2. **RS-485 converter not powered**
   - Converter needs power to work
   - Check power LED or measure voltage

3. **DE/RE pin not connected**
   - Without DE/RE, converter won't switch direction
   - Responses won't get back to Pi5

4. **Wiring reversed**
   - RO → GPIO 19 (not GPIO 15)
   - DI ← GPIO 15 (not GPIO 19)

5. **Pin conflict**
   - GPIO 19 might be used for something else (DW3000_WKUP)
   - Check if there's a jumper or configuration

## Next Steps

1. **Check board pinout:** Find which physical pins on J10 are GPIO 15 and 19
2. **Verify wiring with multimeter:** Test continuity for all connections
3. **Check converter power:** Ensure converter is powered
4. **Test DE/RE pin:** Measure voltage on P0.06 during communication
5. **Try direct connection:** Bypass RS-485 converter to test J10 pins directly

