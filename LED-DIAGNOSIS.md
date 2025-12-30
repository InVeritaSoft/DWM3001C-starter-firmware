# LED Diagnosis Guide

## LED Mapping (DWM3001CDK Board)

| Physical LED | Pin | BSP_LED | Expected Color | Firmware Meaning |
|--------------|-----|---------|----------------|-----------------|
| D9           | P0.04 | LED_0 | Red | Error/Startup indicator |
| D10          | P0.05 | LED_1 | Orange | RX - Command received |
| D11          | P0.22 | LED_2 | Green | TX - Response sent |
| D12          | P0.14 | LED_3 | Blue | UART initialized |

## Expected Behavior

### On Power-On (Normal Startup):
1. **D9 (Red)**: Blinks 3 times, then turns OFF
2. **D12 (Blue)**: Turns ON (indicates UART initialized)
3. **D10 (Orange)**: OFF (no commands yet)
4. **D11 (Green)**: OFF (no responses yet)

### When Command Received:
- **D10 (Orange)**: Blinks briefly (50ms)

### When Response Sent:
- **D11 (Green)**: Blinks briefly (during transmission)

### If UART Init Fails:
- **D9 (Red)**: Blinks rapidly 20 times, then stays ON

## Current Status Analysis

### TX Node:
- ✅ **D12 (Blue) ON**: UART initialized successfully
- ⚠️ **D9 (Red) ON**: Should be OFF after startup - firmware may be stuck
- ❌ **D10/D11**: No activity = no commands being received/processed

### RX Node:
- ✅ **D12 (Blue) ON**: UART initialized successfully  
- ⚠️ **D9 (Red) ON**: Should be OFF after startup - firmware may be stuck
- ❌ **D10/D11**: No activity = no commands being received/processed

## Diagnosis

**Good Signs:**
- D12 (Blue) is ON = UART hardware initialized correctly
- Firmware is running (LEDs are responding)

**Problems:**
- D9 (Red) should turn OFF after startup but is staying ON
- No command/reponse activity (D10/D11 not blinking)
- No serial responses received

## Possible Causes

1. **Firmware stuck in initialization**: D9 staying ON suggests firmware didn't complete startup sequence
2. **RS-485 direction control**: UART works (D12 ON) but responses can't reach PC
3. **Commands not reaching firmware**: RS-485 transceiver may not be switching to RX mode
4. **Firmware crashed after UART init**: May have crashed during DW3000 initialization

## Next Steps

1. **Check serial terminal for startup messages**:
   - Power cycle board
   - Open serial terminal (PuTTY/Tera Term)
   - Configure: COM17/COM18, 115200, 8N1
   - Look for: `OK STARTUP V2`, `OK DW3000_READY`, `OK MAIN_LOOP`

2. **Test command reception**:
   - Send `PNG\r\n` via serial terminal
   - Watch D10 (Orange) - should blink if command received
   - Watch D11 (Green) - should blink if response sent

3. **Check RS-485 hardware**:
   - Verify transceiver is connected
   - Check DE/RE pins (direction control)
   - Verify A+/B- wiring
   - Check termination resistors

4. **Verify firmware version**:
   - Ensure orchestrator v2 is flashed
   - Check `example_selection.h` and `main.c` configuration

