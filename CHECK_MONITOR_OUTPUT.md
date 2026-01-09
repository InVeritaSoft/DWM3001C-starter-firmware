# Check Monitor Output - Critical Diagnostic

## What to Look For

### When You Plug In the Device

**In the serial monitor (COM15), you should see:**

**Orchestrator v2 firmware:**
```
OK STARTUP V2
UART INIT OK
[DBG] UART init succeeded
[DBG] uart_init entry: rx=15 tx=14 rts=4 cts=5
[DBG] APP_UART_FIFO_INIT result: err=0x00000000
[DBG] UART PSEL: RXD=15 TXD=14 RTS=4 CTS=5 ENABLE=4
OK DW3000_READY
OK MAIN_LOOP
```

### What This Tells Us

**If you see startup messages:**
- ✅ Firmware is running
- ✅ UART TX is working (can send data)
- Need to check if UART RX is working

**If you DON'T see startup messages:**
- ❌ Firmware might not be running
- ❌ Wrong COM port
- ❌ UART TX not working

### When You Send PNG Command

**Watch for:**
1. **Orange LED blinks** = UART RX is working! (firmware received command)
2. **Green LED blinks** = UART TX is working! (firmware sent response)
3. **No LED activity** = UART RX not working (GPIO/configuration issue)

## Test Sequence

**1. Start monitor:**
```powershell
python Zephyr\tests\serial_monitor.py COM15
```

**2. Plug in the device**
- Watch for startup messages
- Note what appears

**3. In another terminal, send command:**
```powershell
.\Scripts\test-uart-rx.ps1 COM15
```

**4. Watch:**
- Monitor window: Any response?
- Board: Does Orange LED blink? (RX working)
- Board: Does Green LED blink? (TX working)

## Diagnosis Based on Results

### Scenario 1: Startup messages appear, Orange LED blinks, but no response
- **Problem:** UART RX works, but TX doesn't
- **Fix:** Check UART TX pin configuration or send_response() function

### Scenario 2: Startup messages appear, but no LED blink, no response
- **Problem:** UART RX not working
- **Fix:** GPIO configuration issue (RX pin pullup, pin assignment)

### Scenario 3: No startup messages at all
- **Problem:** Firmware not running or wrong port
- **Fix:** Check firmware is flashed, verify COM port

### Scenario 4: Startup messages appear, LED blinks, response appears
- **Status:** Everything working! ✅

## Quick Test

**Run this to see everything:**
```powershell
.\Scripts\test-uart-rx.ps1 COM15
```

This will:
- Check for startup messages
- Send PNG command
- Tell you what to watch for
- Diagnose the issue
