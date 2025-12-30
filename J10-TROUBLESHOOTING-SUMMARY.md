# J10 Pin Troubleshooting Summary

## Problem Confirmed
- ✅ **USB (Putty) works** - Firmware responds correctly
- ✅ **Blue LED blinks** when typing PNG via USB
- ❌ **J10 pins (GPIO 15/19) don't work** via RS-485

## Key Finding from Context7 Research

According to Nordic nRF SDK documentation:
- **nRF UART pins are configurable** - Most GPIO pins can be used for UART
- GPIO 15 (P0.15) and GPIO 19 (P0.19) **should be valid** UART pins
- The firmware code is correct (using `app_uart` with UART0)

## Most Likely Root Causes

### 1. **Wrong Physical Pins on J10 Connector** ⚠️ HIGH PROBABILITY
**Issue:** GPIO 15 and GPIO 19 might not be on the physical pins you think they are.

**Action Required:**
- Check DWM3001CDK board schematic/datasheet
- Find which physical pins on J10 connector are P0.15 and P0.19
- Verify your wiring matches the actual pin locations

### 2. **RS-485 Converter Not Powered** ⚠️ HIGH PROBABILITY
**Issue:** Converter needs power to function.

**Check:**
- Does converter have power LED? Is it on?
- Measure voltage on converter VCC pin (should be 3.3V or 5V)
- Check converter datasheet for power requirements

### 3. **DE/RE Pin Not Connected** ⚠️ HIGH PROBABILITY
**Issue:** Without DE/RE control, converter won't switch direction.

**Check:**
- Is P0.06 connected to converter DE/RE pin?
- Measure voltage on P0.06:
  - Should be LOW (0V) when idle (RX mode)
  - Should go HIGH (3.3V) briefly when board sends response
- If voltage doesn't change, DE/RE is not connected or firmware not controlling it

### 4. **Wiring Reversed** ⚠️ MEDIUM PROBABILITY
**After pin swap, correct wiring:**
- RO (from converter) → GPIO 19 (RX pin)
- DI (to converter) ← GPIO 15 (TX pin)

**Verify with multimeter:**
- Test continuity: RO pin → GPIO 19
- Test continuity: DI pin → GPIO 15

### 5. **Pin Conflict** ⚠️ LOW PROBABILITY
**Note:** GPIO 19 (P0.19) is also used as `DW3000_WKUP_Pin`

**Check:**
- Is there a jumper or switch that routes GPIO 19 elsewhere?
- Is GPIO 19 being used for UWB chip wake-up instead of UART?

## Diagnostic Steps

### Step 1: Verify Physical Pin Locations
1. Get DWM3001CDK board schematic
2. Find J10 connector pinout
3. Identify which physical pins are P0.15 and P0.19
4. Verify your wiring matches these pins

### Step 2: Test RS-485 Converter Power
```bash
# Use multimeter to check:
- VCC pin on converter should have voltage (3.3V or 5V)
- GND pin should be connected to board GND
- Power LED (if present) should be on
```

### Step 3: Test DE/RE Pin Control
```bash
# With board powered and firmware running:
1. Measure voltage on P0.06 (should be LOW ~0V)
2. Send command via RS-485 that triggers response
3. Watch voltage on P0.06 (should briefly go HIGH ~3.3V)
4. If voltage doesn't change, DE/RE not connected or not working
```

### Step 4: Test Direct Connection (Bypass RS-485)
```bash
# Connect Pi5 directly to board J10 pins:
- Pi5 TX → Board GPIO 19 (RX)
- Pi5 RX ← Board GPIO 15 (TX)
- GND → GND

# Test communication:
- If this works: Problem is RS-485 converter
- If this doesn't work: Problem is J10 pins or firmware config
```

### Step 5: Check Board Documentation
Look for:
- J10 pinout diagram
- Any jumpers/switches affecting GPIO 15/19
- UART pin configuration notes
- Any warnings about pin conflicts

## Quick Verification Checklist

- [ ] Verified which physical pins on J10 are GPIO 15 and 19
- [ ] RS-485 converter has power (LED on or voltage measured)
- [ ] DE/RE pin connected to P0.06
- [ ] RO pin connected to GPIO 19 (not GPIO 15)
- [ ] DI pin connected to GPIO 15 (not GPIO 19)
- [ ] GND connected between converter and board
- [ ] Tested continuity with multimeter for all connections
- [ ] Verified P0.06 voltage changes when board sends response

## Next Steps

1. **Find board schematic** - Identify actual J10 pin locations
2. **Verify converter power** - Ensure converter is powered
3. **Test DE/RE pin** - Measure voltage on P0.06 during communication
4. **Try direct connection** - Bypass RS-485 converter to test J10 pins
5. **Check for pin conflicts** - Verify GPIO 19 isn't used elsewhere

## Expected Behavior When Working

**When sending command via RS-485:**
1. Pi5 sends → USB RS-485 adapter TX LED flashes ✓ (you see this)
2. RS-485 converter RX LED flashes ✓ (you see this)
3. **Board Orange LED (LED 1) should blink** ← Check this!
4. Board processes command
5. **Board Green LED (LED 2) should blink** ← Check this!
6. **P0.06 should go HIGH briefly** ← Measure this!
7. Board sends response
8. Response reaches Pi5

**If Orange LED doesn't blink:** Board not receiving (wiring issue)
**If Green LED doesn't blink:** Board not sending (DE/RE issue)
**If P0.06 doesn't change:** DE/RE not connected or not working

