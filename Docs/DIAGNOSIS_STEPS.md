# UART Diagnosis Steps

## Critical Questions to Answer

### 1. Is the firmware running?

**Test:** Check for startup messages

**Steps:**
1. Open serial monitor: `python Zephyr\tests\serial_monitor.py COM15` (or use the script below)
2. **Unplug the device**
3. **Plug it back in**
4. **Watch for startup messages**

**Expected output:**
```
OK STARTUP V2
UART INIT OK
[DBG] UART init succeeded
OK DW3000_READY
OK MAIN_LOOP
```

**If you see startup messages:**
- ✅ Firmware is running
- ✅ UART TX is working (can send data)
- Need to check UART RX

**If you DON'T see startup messages:**
- ❌ Firmware might not be running
- ❌ Wrong COM port
- ❌ UART TX not working
- ❌ Firmware not flashed with latest changes

### 2. Is UART RX working?

**Test:** Watch LED when sending commands

**Steps:**
1. Send PNG command: `.\Scripts\send-command.ps1 COM15 PNG`
2. **Watch the board's Orange LED (LED 1)**

**If Orange LED blinks:**
- ✅ UART RX is working (firmware received command)
- Problem might be: TX not sending response, or command not processed

**If Orange LED does NOT blink:**
- ❌ UART RX is NOT working
- Problem: GPIO configuration issue, RX pin not configured correctly

### 3. Is UART TX working (for responses)?

**Test:** Check if responses are sent

**If you see startup messages but no command responses:**
- ✅ UART TX works (startup messages sent)
- ❌ But responses not sent (might be command processing issue)

## Quick Diagnostic Script

Run this comprehensive test:

```powershell
.\Scripts\diagnose-uart-complete.ps1 COM15
```

This will:
1. Check for startup messages (asks you to replug device)
2. Test command sending
3. Tell you exactly what's working and what's not

## Most Likely Issues

### Issue 1: Firmware Not Reflashed
**Symptom:** No startup messages, no responses
**Fix:** Reflash firmware with latest changes

### Issue 2: UART RX Not Working
**Symptom:** Startup messages appear, but Orange LED doesn't blink when sending commands
**Fix:** GPIO RX pin configuration issue (already fixed in code, but might need reflash)

### Issue 3: Command Processing Issue
**Symptom:** Orange LED blinks (RX works), but no response
**Fix:** Command parser or response sending issue

## Next Steps

1. **Run the diagnostic script** to identify the exact issue
2. **Check if firmware was reflashed** with latest changes
3. **Verify startup messages** appear when plugging in
4. **Watch LED activity** when sending commands
