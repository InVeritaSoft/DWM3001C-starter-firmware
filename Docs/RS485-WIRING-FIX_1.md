# RS-485 Wiring Fix - Pin Connection Issue

## Problem Description

**Symptom**: RS-485 USB adapter sends data → RS-485 to TTL converter RX LED blinks → but firmware doesn't receive data → UWB transmission doesn't start.

## Root Cause

**Critical Wiring Error**: The RS-485 to TTL converter's output is connected to the wrong pin.

### Correct Pin Mapping (J10 Connector)

The DWM3001CDK board uses J10 connector with UART pins:
- **Pin 8 on J10** = **GPIO 14 (P0.14)** = **UART TX pin** (sends data TO RS-485 converter)
- **Pin 10 on J10** = **GPIO 15 (P0.15)** = **UART RX pin** (receives data FROM RS-485 converter)

### Common Mistake

Many users connect to wrong pins:
- ❌ RS-485 converter RO output → GPIO 14 (WRONG! GPIO 14 is TX, not RX)
- ❌ Using pins that aren't routed to UART hardware

### Correct Connection

- ✅ **RS-485 converter RO (Receive Out)** → **GPIO 15 (P0.15)** - RX pin (Pin 10 on J10)
- ✅ **RS-485 converter DI (Data In)** ← **GPIO 14 (P0.14)** - TX pin (Pin 8 on J10)

## Correct Wiring Diagram

```
RS-485 USB Adapter          RS-485 to TTL Converter          DWM3001CDK Board
──────────────────          ──────────────────────          ────────────────
                                                             
A+ (differential +) ────────> A+ ──────────────────────────> (RS-485 bus)
B- (differential -) ────────> B- ──────────────────────────> (RS-485 bus)
GND ───────────────────────> GND ─────────────────────────> GND (Pin 6)
                                                             
                                                             GPIO 15 (RX) <─── RO (Receive Out - TTL output)
                                                             GPIO 14 (TX) ────> DI (Data In - TTL input)
                                                             (Pin 10 on J10)   (Pin 8 on J10)
```

## RS-485 to TTL Converter Pin Labels

Most RS-485 to TTL converters have these labels:
- **RO** (Receive Out) or **TXD** or **DO** = TTL output (sends data TO microcontroller) → Connect to board RX pin
- **DI** (Data In) or **RXD** = TTL input (receives data FROM microcontroller) ← Connect from board TX pin

## Connection Checklist

1. ✅ **RS-485 converter RO (Receive Out)** → **GPIO 15 (P0.15)** on DWM3001CDK (RX pin - Pin 10 on J10)
2. ✅ **RS-485 converter DI (Data In)** ← **GPIO 14 (P0.14)** on DWM3001CDK (TX pin - Pin 8 on J10)
3. ✅ **RS-485 converter GND** → **GND** on DWM3001CDK (Pin 6)
4. ✅ **RS-485 bus A+** → Connected to other nodes
5. ✅ **RS-485 bus B-** → Connected to other nodes

## Verification Steps

After correcting the wiring:

1. **Power on the board** - Blue LED (LED 3) should turn on (UART initialized)

2. **Check startup messages** - Connect serial terminal to USB port:
   ```
   OK STARTUP V2
   OK DW3000_READY
   OK MAIN_LOOP
   ```

3. **Send PING command** via RS-485:
   ```
   PNG
   ```
   Should receive: `OK`

4. **Watch LEDs**:
   - Orange LED (LED 1) blinks when command received
   - Green LED (LED 2) blinks when response sent

5. **Send START_TEST command**:
   ```
   CFG ch=5 rate=6m8 pl=128 len=64 pwr=5 rate_hz=100
   STRT
   ```
   Should receive: `OK START` and UWB packets should transmit

## Code Fix Applied

The firmware has been updated to reset UART pins before initialization:

```c
/* Reset UART pins to default state before initialization */
nrf_gpio_cfg_default(UART_0_RX_PIN);  // GPIO 15 (P0.15) - RX pin (Pin 10 on J10)
nrf_gpio_cfg_default(UART_0_TX_PIN);  // GPIO 14 (P0.14) - TX pin (Pin 8 on J10)
nrf_delay_ms(10);  // Small delay to ensure pin state is stable
```

This removes any pull-up/pull-down resistors that might interfere with UART signals.

## Troubleshooting

### Still Not Working?

1. **Double-check wiring**:
   - Converter RO (Receive Out) → Board GPIO 15 (P0.15) - RX pin (Pin 10 on J10)
   - Converter DI (Data In) ← Board GPIO 14 (P0.14) - TX pin (Pin 8 on J10)
   - NOT the other way around!

2. **Check baud rate**: Must be 115200

3. **Check RS-485 termination**: 120Ω resistors at each end of the bus

4. **Verify converter power**: RS-485 converter needs power (usually 5V or 3.3V)

5. **Check converter direction**: Some converters have auto-direction switching

## Summary

**The key issue**: UART pins must match J10 connector routing. Pin 8 on J10 = GPIO 14 (TX), Pin 10 on J10 = GPIO 15 (RX). Data from the RS-485 converter RO pin must go to **GPIO 15 (RX pin - Pin 10 on J10)**, not GPIO 14.

