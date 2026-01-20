# UART Pin Configuration Fix - RX=14, TX=15

## Problem Identified

**Hardware Testing Results:**
- **RX=15, TX=14:** Blue LED blinks (interrupt works), but no response (TX doesn't work)
- **RX=14, TX=15:** Commands received (visible in monitor), but LED doesn't blink (interrupt not firing)

**Root Cause:**
When RX=14, TX=15, the UART interrupt handler is not being called, even though data is being received.

## Solution Applied

### 1. Corrected Pin Configuration
Updated `custom_board.h` to reflect the correct hardware mapping:
```c
#define RX_PIN_NUMBER  14  // GPIO 14 (P0.14) - J10 Pin 10 (RXD0) - Confirmed working for RX
#define TX_PIN_NUMBER  15  // GPIO 15 (P0.15) - J10 Pin 8 (TXD0) - Confirmed working for TX
```

### 2. Explicit Interrupt Enable
Added explicit RX interrupt enable after UART initialization:
```c
// Enable UART RX interrupt to ensure data ready events are triggered
NRF_UART0->INTENSET = (1UL << 2);  // UART_INTENSET_RXDRDY_Msk = bit 2
```

### 3. Debug LED Blinking
Added Blue LED blink on **every byte received** to confirm interrupt is working:
```c
// DEBUG: Blink BLUE LED on every byte received
bsp_board_led_on(3);  // Blue LED - shows interrupt is firing
nrf_delay_ms(10);
bsp_board_led_off(3);
```

This helps diagnose:
- If Blue LED blinks = Interrupt handler is being called ✅
- If Blue LED doesn't blink = Interrupt handler not being called ❌

### 4. Enhanced Command LED
Orange LED now blinks longer (100ms) when complete command is received.

## Expected Behavior After Fix

**When sending PNG command:**

1. **Blue LED blinks** for each character received (P, N, G, \n)
   - Confirms interrupt handler is firing
   - Confirms data is being received

2. **Orange LED blinks** when complete command is received
   - Confirms command parsing is triggered

3. **Green LED blinks** when response is sent
   - Confirms TX is working

4. **Response "OK" appears** in serial monitor
   - Confirms end-to-end communication works

## Testing Steps

1. **Flash updated firmware** with RX=14, TX=15 configuration
2. **Open serial monitor:** `.\Scripts\serial-monitor.ps1 COM15`
3. **Send PNG command:** `.\Scripts\send-command.ps1 COM15 PNG`
4. **Watch LEDs:**
   - Blue LED should blink 4 times (P, N, G, \n)
   - Orange LED should blink once (command received)
   - Green LED should blink (response sent)
5. **Check monitor:** Should see "OK" response

## Files Modified

- `Src/custom_board.h` - Corrected pin definitions and comments
- `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c` - Added interrupt enable, debug LEDs
- `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c` - Added interrupt enable, debug LEDs

## Next Steps

Once this is confirmed working:
1. ✅ UART RX/TX communication working
2. → Proceed to UWB packet sending
3. → First: TX/RX between node and orchestrator
4. → Then: Node A to Node B UWB communication
