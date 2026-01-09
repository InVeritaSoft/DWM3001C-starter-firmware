# UART Timing Fix - Resolves Intermittent Response Issue

## Problem

**Symptom:**
- Firmware responds intermittently (sometimes works, sometimes doesn't)
- Holding reset button helps (device responds after reset)
- "?" responses indicate partial/corrupted data
- Only 2 out of 22 commands succeeded

**Root Cause:**
1. **Too short delay after GPIO reset** (1ms) - hardware needs more time to stabilize
2. **No delay after UART init** - UART hardware needs time to fully initialize
3. **No UART disable before init** - UART might be in an inconsistent state

## Solution

### Changes Made

**1. Disable UART before initialization:**
```c
// Disable UART to ensure clean state (mimics reset button behavior)
NRF_UART0->ENABLE = 0;  // Disable UART
nrf_delay_ms(10);  // Wait for UART to fully disable
```

**2. Increased delay after GPIO reset:**
```c
nrf_gpio_cfg_default(UART_0_RX_PIN);
nrf_gpio_cfg_default(UART_0_TX_PIN);
nrf_delay_ms(20);  // Increased from 1ms to 20ms - ensures pin state is stable
```

**3. Added delay after pullup configuration:**
```c
nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);
nrf_delay_ms(5);  // Small delay after pullup configuration
```

**4. Added delay after UART initialization:**
```c
APP_UART_FIFO_INIT(...);
nrf_delay_ms(50);  // Allow UART to stabilize after initialization
```

## Why Holding Reset Helped

**When you hold reset:**
1. UART is disabled (ENABLE = 0)
2. GPIO pins are reset to default state
3. When released, everything starts from a clean state
4. This gives the hardware time to stabilize

**The fix mimics this behavior:**
- Explicitly disable UART before init
- Longer delays ensure hardware is ready
- Clean initialization sequence

## Expected Results

**After this fix:**
- ✅ Consistent responses (no more intermittent failures)
- ✅ No need to hold reset button
- ✅ Reliable UART communication
- ✅ All commands should get responses

## Testing

**Test sequence:**
1. Flash the updated firmware
2. Plug in device (no need to hold reset)
3. Send PNG commands repeatedly
4. Should get consistent "OK" responses

**Expected output:**
```
[14:34:00] Sending PNG command... ✓ OK
[14:34:02] Sending PNG command... ✓ OK
[14:34:04] Sending PNG command... ✓ OK
[14:34:06] Sending PNG command... ✓ OK
```

## Files Modified

- `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c`
- `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c`

## Technical Details

**Timing sequence:**
1. Disable UART: 10ms delay
2. Reset GPIO pins: 20ms delay (was 1ms)
3. Configure RX pullup: 5ms delay
4. Initialize UART: 50ms delay (new)

**Total initialization time:** ~85ms (acceptable for startup)

**Why these delays:**
- **10ms UART disable:** Ensures UART hardware fully stops
- **20ms GPIO reset:** Allows pin state to stabilize (critical for reliable operation)
- **5ms pullup config:** Ensures pullup resistor is active
- **50ms UART init:** Allows UART hardware to fully initialize and be ready
