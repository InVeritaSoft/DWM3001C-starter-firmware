# Debug: No Response Received

## Problem
- Commands are received (visible in serial monitor)
- But no response is sent back
- RX=14, TX=15 configuration

## Debugging Added

### 1. Extensive Logging in Event Handler
- Logs every byte received: `[RX] byte=0xXX 'X'`
- Logs complete command: `[RX] CMD: 'PNG'`
- Logs errors: `[RX] app_uart_get err=0xXXXX`

### 2. Test Response for Empty Commands
- Sends "OK TEST" even for empty commands (just newline)
- Helps verify TX is working

### 3. Enhanced PNG Response
- Sends "OK" + "\r\n" to ensure complete response
- Logs before and after sending

### 4. Interrupt Enable Verification
- Logs INTENSET register value
- Ensures interrupt is enabled AFTER UART is enabled

## What to Check in Serial Monitor

**When you send PNG, you should see:**

1. **Startup messages** (if device just powered on):
   ```
   OK STARTUP V2
   UART INIT OK
   [DBG] UART init succeeded
   [DBG] UART INTENSET=0x00000004 (bit 2 should be set)
   ```

2. **When PNG is sent:**
   ```
   [RX] byte=0x50 'P'
   [RX] byte=0x4E 'N'
   [RX] byte=0x47 'G'
   [RX] byte=0x0A '\n'  (or 0x0D '\r')
   [RX] CMD: 'PNG'
   [DBG] PNG received, sending OK response
   [DBG] PNG response sent
   OK
   ```

## Diagnosis Based on Output

### If you see NO [RX] messages:
- ❌ Interrupt handler is NOT being called
- Problem: Interrupt not enabled or not firing
- Fix: Check INTENSET register, verify interrupt priority

### If you see [RX] messages but NO [DBG] PNG messages:
- ✅ Interrupt handler IS working
- ❌ Command parsing is failing
- Problem: Command buffer issue or string comparison failing
- Fix: Check rx_buffer contents, verify command string

### If you see [DBG] PNG messages but NO "OK" response:
- ✅ Interrupt handler IS working
- ✅ Command parsing IS working
- ❌ send_response() is failing
- Problem: TX not working or response not being sent
- Fix: Check send_response() function, verify TX pin

### If you see everything but response is garbled:
- ✅ Everything is working
- Problem: Baud rate mismatch or timing issue
- Fix: Verify baud rate, check timing

## Next Steps

1. **Flash updated firmware** with debug logging
2. **Open serial monitor:** `.\Scripts\serial-monitor.ps1 COM15`
3. **Send PNG command:** `.\Scripts\send-command.ps1 COM15 PNG`
4. **Check what appears in monitor:**
   - Do you see [RX] messages? (interrupt working)
   - Do you see [DBG] PNG messages? (parsing working)
   - Do you see "OK" response? (TX working)

## LED Indicators

- **Blue LED blinks** = Interrupt handler called (byte received)
- **Orange LED blinks** = Complete command received
- **Green LED blinks** = Response being sent

If Blue LED doesn't blink, interrupt handler isn't being called.
