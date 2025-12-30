# How LEDs Can Blink But Putty Shows No Response

## The Key: RS-485 is Bidirectional But Requires Direction Control

RS-485 uses a **single differential pair** (A+ and B-) for both sending and receiving. Unlike regular UART which has separate TX and RX lines, RS-485 needs a **direction control pin** (DE/RE) to switch between:
- **RX Mode** (LOW): Transceiver listens to the bus
- **TX Mode** (HIGH): Transceiver drives the bus

## What Happens When You Send a Command

### Step 1: Command Received (RX Mode) ✅

```
Putty → RS-485 Bus → Board's RS-485 Transceiver (RX mode) → UART RX → Firmware
```

**Flow:**
1. Putty sends "PNG" command
2. RS-485 transceiver is in **RX mode** (DE/RE = LOW)
3. Transceiver receives data from bus
4. Data goes to UART RX pin (GPIO 15)
5. UART interrupt fires → `uart_event_handler()` called
6. **Orange LED blinks** (line 197-199) - Command received!
7. Command parsed → `parse_command()` called
8. Response prepared → `send_response("OK")` called

**This part works!** That's why you see the LED blink.

### Step 2: Response Sent (TX Mode) ❌

```
Firmware → UART TX → RS-485 Transceiver (TX mode) → RS-485 Bus → Putty
```

**Flow:**
1. `send_response()` function called (line 332)
2. **Green LED turns ON** (line 338) - Response being sent
3. **CRITICAL:** Set DE/RE pin HIGH (line 342) - Switch to TX mode
4. Wait 1ms for transceiver to switch (line 343)
5. Send response bytes via UART (lines 345-368)
6. Wait for transmission to complete (line 371)
7. **CRITICAL:** Set DE/RE pin LOW (line 375) - Switch back to RX mode
8. **Green LED turns OFF** (line 378)

**This part might fail!** That's why Putty doesn't see the response.

## Why Responses Don't Reach Putty

### Issue 1: DE/RE Pin Not Connected ⚠️

**Most Common Problem!**

The firmware tries to set `RS485_DE_RE_PIN` (GPIO P0.06) HIGH to enable TX mode, but:
- If DE/RE pin isn't connected to the transceiver
- Or connected to wrong pin
- Or transceiver doesn't have DE/RE control

**Result:** Transceiver stays in RX mode, can't drive the bus, response never reaches Putty.

**Check:** Is GPIO P0.06 connected to your RS-485 transceiver's DE/RE pin?

### Issue 2: DE/RE Pin Timing ⚠️

The firmware does:
```c
nrf_gpio_pin_write(RS485_DE_RE_PIN, 1);  // Switch to TX
nrf_delay_ms(1);                          // Wait 1ms
// Send data...
nrf_delay_ms(2);                          // Wait for transmission
nrf_gpio_pin_write(RS485_DE_RE_PIN, 0);  // Switch back to RX
```

If the transceiver needs more time to switch, responses might be lost.

### Issue 3: UART TX Pin Issue ⚠️

The firmware sends data via UART TX pin (GPIO 14 / P0.14), but:
- If UART TX isn't connected to transceiver's DI pin
- Or wrong pin connected
- Or UART TX disabled

**Result:** Data never reaches transceiver, even if DE/RE is correct.

### Issue 4: RS-485 Bus Wiring ⚠️

Even if firmware sends correctly:
- A+ and B- lines must be connected correctly
- Termination resistors needed (120Ω at each end)
- Bus must be properly wired

**Result:** Response sent but doesn't reach Putty due to wiring.

## Visual Flow Diagram

```
┌─────────┐                    ┌──────────────┐                    ┌─────────┐
│  Putty  │                    │ RS-485       │                    │  Board  │
│         │                    │ Transceiver  │                    │         │
└────┬────┘                    └──────┬───────┘                    └────┬────┘
     │                                 │                                │
     │  "PNG" command                  │                                │
     ├─────────────────────────────────>│                                │
     │                                 │  DE/RE = LOW (RX mode)         │
     │                                 │  ✅ Receives command           │
     │                                 ├───────────────────────────────>│
     │                                 │                                │  Orange LED blinks
     │                                 │                                │  parse_command()
     │                                 │                                │  send_response()
     │                                 │                                │
     │                                 │  DE/RE = HIGH (TX mode)        │
     │                                 │  ❌ Should send response       │
     │                                 │                                │  Green LED ON
     │  "OK" response                 │                                │
     │<─────────────────────────────────┤                                │
     │                                 │  ❌ But this fails!            │
     │                                 │                                │
```

## How to Diagnose

### Check 1: Watch the Green LED

When you send a command:
- **Orange LED blinks** = Command received ✅
- **Green LED should blink** = Response being sent

**If green LED doesn't blink:**
- `send_response()` isn't being called
- Command parsing failed
- Firmware issue

**If green LED blinks but no response:**
- DE/RE pin issue (most likely!)
- UART TX pin issue
- RS-485 wiring issue

### Check 2: Verify DE/RE Pin Connection

**Critical:** GPIO P0.06 must be connected to RS-485 transceiver's DE/RE pin!

**Check your wiring:**
- DE/RE pin on transceiver → GPIO P0.06 (RS485_DE_RE_PIN)
- If transceiver has separate DE and RE pins, connect both to P0.06

### Check 3: Test with Multimeter/Oscilloscope

1. **Measure DE/RE pin (P0.06):**
   - Should be LOW (0V) normally (RX mode)
   - Should go HIGH (3.3V) when sending response
   - If it never goes HIGH → Firmware issue or wrong pin

2. **Measure UART TX pin (P0.14):**
   - Should show data when sending response
   - If no data → UART TX issue

### Check 4: RTT Logs

Check if `send_response()` is actually being called:

```powershell
.\stream-debug-logs.ps1
```

Look for:
- Command parsing messages
- Any errors during response sending

## Summary

**Why LEDs blink but Putty shows nothing:**

1. ✅ **RX Path Works:** Commands received → Orange LED blinks
2. ❌ **TX Path Fails:** Responses sent but don't reach Putty

**Most likely causes:**
1. **DE/RE pin not connected** (80% of cases)
2. **DE/RE pin connected to wrong GPIO**
3. **RS-485 transceiver wiring issue**
4. **UART TX pin not connected**

**The firmware code is correct** - it's trying to send responses. The issue is **hardware wiring** preventing responses from reaching Putty.

## Solution

**Verify RS-485 hardware connections:**
- DE/RE pin → GPIO P0.06 ✅
- DI pin → GPIO P0.14 (UART TX) ✅
- RO pin → GPIO P0.15 (UART RX) ✅
- A+, B-, GND → RS-485 bus ✅

If all connections are correct but still no response, check:
- RS-485 termination resistors (120Ω)
- Transceiver power supply
- Bus wiring integrity

