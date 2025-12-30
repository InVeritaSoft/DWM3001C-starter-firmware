# Putty No Response - Troubleshooting Guide

## Expected Behavior

When orchestrator firmware boots, it should send these messages automatically:
1. `OK STARTUP V2`
2. `OK DW3000_READY` (or `ERR DW3000_TIMEOUT` if hardware issue)
3. `OK MAIN_LOOP`

**If you don't see ANY of these messages, the firmware isn't communicating.**

## Quick Diagnostic Steps

### Step 1: Check RTT Logs (Firmware Running?)

```powershell
.\stream-debug-logs.ps1
```

**Look for:**
- `ORCHESTRATOR TX v2.0` or `ORCHESTRATOR RX v2.0`
- `UART INIT OK` or `UART INIT FAILED`
- `OK STARTUP V2`
- `OK DW3000_READY`

**If you see firmware messages:** Firmware is running, but UART/RS-485 isn't working
**If you see nothing:** Firmware might not be running (check flash)

### Step 2: Check LED Behavior

**On boot, you should see:**
- Red LED: Blinks 2 times (firmware started)
- Orange LED: Blinks 2 times (UART initialized)
- Green LED: Brief flash (startup message sent)

**If LEDs don't blink:** Firmware might not be running

### Step 3: Verify Putty Settings

**Required Putty Settings:**
- **Connection type:** Serial
- **Serial line:** COM11 (or your COM port)
- **Speed:** 115200
- **Data bits:** 8
- **Stop bits:** 1
- **Parity:** None
- **Flow control:** None

**Common mistakes:**
- Wrong COM port
- Wrong baud rate (not 115200)
- Flow control enabled (should be None)

### Step 4: Check COM Port

```powershell
.\list-com-ports.ps1
```

Verify the COM port matches what you're using in Putty.

### Step 5: Test with Diagnostic Script

```powershell
.\diagnose-putty-no-response.ps1
```

This will:
- Check RTT logs
- List COM ports
- Test serial communication
- Show what's wrong

## Common Issues and Solutions

### Issue 1: Wrong COM Port

**Symptoms:**
- Putty opens but no data
- No response to commands

**Solution:**
1. Check Device Manager → Ports (COM & LPT)
2. Find your board (might be "J-Link" or "nRF52833")
3. Note the COM port number
4. Use that in Putty

### Issue 2: Wrong Baud Rate

**Symptoms:**
- Garbled characters
- No response

**Solution:**
- Must be exactly **115200**
- Check Putty settings

### Issue 3: RS-485 Hardware Not Connected

**Symptoms:**
- Firmware running (RTT logs show messages)
- LEDs blink
- But Putty shows nothing

**Solution:**
- Verify RS-485 transceiver is connected:
  - A+ → RS-485 A+ line
  - B- → RS-485 B- line
  - GND → Ground
- Check RS-485 termination resistors (120Ω at each end)

### Issue 4: Firmware Not Running

**Symptoms:**
- No RTT logs
- No LED activity
- Putty shows nothing

**Solution:**
1. Verify flash succeeded:
   ```powershell
   make flash
   # Should show "Programming successful"
   ```

2. Press reset button on board

3. Check if firmware is actually running:
   ```powershell
   .\stream-debug-logs.ps1
   ```

### Issue 5: UART Initialization Failed

**Symptoms:**
- RTT logs show "UART INIT FAILED"
- No startup messages in Putty

**Solution:**
- Check UART pin configuration
- Verify pins aren't conflicting
- Check custom_board.h for pin definitions

### Issue 6: Port Already Open

**Symptoms:**
- Error when opening port
- "Access denied" or "Port in use"

**Solution:**
- Close Putty
- Close any other serial terminal
- Close diagnostic scripts
- Try again

## Step-by-Step Recovery

### If Nothing Works:

1. **Verify firmware is flashed:**
   ```powershell
   make flash
   ```

2. **Check RTT logs:**
   ```powershell
   .\stream-debug-logs.ps1
   ```
   - If you see firmware messages → UART/RS-485 issue
   - If you see nothing → Flash issue

3. **Reset the board:**
   - Press reset button
   - Watch LEDs
   - Check Putty for startup messages

4. **Try different COM port:**
   - Some boards have multiple COM ports
   - Try both if available

5. **Check hardware connections:**
   - RS-485 transceiver connected?
   - Correct pins?
   - Power connected?

## Expected Startup Sequence

**When firmware boots successfully, you should see in Putty:**

```
OK STARTUP V2
OK DW3000_READY
OK MAIN_LOOP
```

**Then when you type `PNG` and press Enter:**

```
PNG
OK
```

**If you see startup messages but no response to PNG:**
- Commands are being received (LED blinks)
- But responses aren't being sent
- Check RS-485 direction control (DE/RE pin)

## Quick Test

Run this diagnostic:

```powershell
.\diagnose-putty-no-response.ps1
```

It will check everything and tell you what's wrong.

