# UART Stops Working After Startup - Diagnosis

## Symptom
- ✅ Firmware responds ONCE when plugging in (startup message)
- ❌ Then stops responding to all commands
- Monitor shows connection but no ongoing data

## Possible Causes

### 1. UART Interrupts Not Enabled/Working

**Check:** The UART event handler might not be receiving interrupts after initialization.

**Fix:** Verify UART interrupts are properly enabled in the initialization.

### 2. Main Loop Blocking UART

**Check:** The main loop might be blocking or preventing UART interrupts.

**Current main loop:**
```c
while (1) {
    if (g_test_running) {
        process_rx_packet();
        Sleep(1);
    } else {
        Sleep(100);  // Long sleep might affect timing
    }
}
```

### 3. UART Buffer Issue

**Check:** UART receive buffer might be getting stuck or not being read.

### 4. Interrupt Priority Issue

**Check:** UART interrupt priority might be too low or blocked by other interrupts.

## What to Check in Monitor

**When you plug in, you should see:**
```
OK STARTUP V2
UART INIT OK
[DBG] UART init succeeded
OK DW3000_READY
OK MAIN_LOOP
```

**If you see these messages:**
- Firmware is running ✅
- UART TX is working ✅
- But UART RX might not be working ❌

**If you DON'T see these messages:**
- Firmware might not be running
- Wrong COM port
- UART TX not working

## Next Steps

1. **Check what the monitor shows** when you plug in
2. **Try sending commands** while monitoring
3. **Check if LED blinks** when sending commands (indicates RX is working)
