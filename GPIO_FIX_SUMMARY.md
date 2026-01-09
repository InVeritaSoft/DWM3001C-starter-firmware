# GPIO Configuration Fix for Orchestrator v2

## Problem Identified

The RX pin pullup was being configured **BEFORE** `uart_init()`, but then inside `uart_init()`, the code calls `nrf_gpio_cfg_default()` which **clears** the pullup configuration.

**Sequence (WRONG):**
1. Configure RX pin pullup (before uart_init)
2. Call uart_init()
3. Inside uart_init(): `nrf_gpio_cfg_default()` ← **CLEARS the pullup!**
4. APP_UART_FIFO_INIT() ← RX pin has no pullup

## Solution

Move the RX pin pullup configuration **INSIDE** `uart_init()`, **AFTER** the pin reset but **BEFORE** `APP_UART_FIFO_INIT()`.

**Sequence (CORRECT):**
1. Call uart_init()
2. Inside uart_init(): `nrf_gpio_cfg_default()` (reset pins)
3. **Configure RX pin pullup** ← After reset, before UART init
4. APP_UART_FIFO_INIT() ← RX pin has pullup configured

## Changes Made

### Files Modified:
- `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c`
- `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c`

### What Changed:

**REMOVED** from main function (before uart_init):
```c
nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);
```

**ADDED** inside uart_init() (after pin reset):
```c
// CRITICAL: Configure RX pin as input with pullup AFTER reset but BEFORE UART init
// This ensures proper pin state before UART takes control (from demo firmware)
// Must be done AFTER cfg_default() to avoid being cleared
nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);
```

## Why This Matters

1. **Prevents floating input** - RX pin needs pullup to avoid noise
2. **Proper pin state** - Pin must be configured before UART takes control
3. **Timing** - Must be after pin reset but before UART initialization

## Next Steps

1. **Rebuild the firmware** with these changes
2. **Flash the new firmware** to the board
3. **Test PNG command** - should now work!

## Verification

After flashing, you should see:
- Startup message: "OK STARTUP V2"
- PNG command responds with: "OK"

If still not working, check:
- Firmware was rebuilt and reflashed
- GPIO pins are 14/15 (TX/RX)
- Hardware connections are correct
