# DWM3001CDK Firmware Modifications

## Overview

The existing DWM3001CDK firmware in `Src/examples/ex_22_orchestrator_v2/` was originally designed to connect directly to an RS485 transceiver. Now that the Arduino Uno handles RS485 direction control (DE/RE pins), the DWM3001CDK firmware needs minor modifications to use simple TTL serial communication.

## Required Changes

### 1. Remove RS485_DE_PIN Handling

The DWM3001CDK firmware currently includes code to control an RS485 transceiver's DE (Driver Enable) pin. This is no longer needed because the Arduino now handles this.

#### Files to Modify

- `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c`
- `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c`

### 2. Specific Code Sections to Remove

#### In `orchestrator_tx_v2.c`:

**Section 1: Remove DE pin configuration in `uart_init()` (lines 264-271)**

Remove this block:
```c
// CRITICAL: Configure RS-485 DE (Driver Enable) pin FIRST (if defined)
// DE pin controls RS-485 transceiver direction:
// - LOW = receive mode (default)
// - HIGH = transmit mode
#ifdef RS485_DE_PIN
nrf_gpio_cfg_output(RS485_DE_PIN);
nrf_gpio_pin_clear(RS485_DE_PIN);  // Set to receive mode (LOW)
nrf_delay_ms(10);  // Wait for transceiver to enter receive mode
test_run_info((unsigned char *)"[DBG] RS-485 DE pin configured (receive mode)");
#else
test_run_info((unsigned char *)"[DBG] RS-485 DE pin not defined - assuming automatic direction control");
#endif
```

**Section 2: Remove DE pin control in `send_response()` - Before TX (lines 440-444)**

Remove this block:
```c
// CRITICAL: Enable RS-485 transmit mode BEFORE sending data (if DE pin defined)
#ifdef RS485_DE_PIN
nrf_gpio_pin_set(RS485_DE_PIN);  // Set DE HIGH = transmit mode
nrf_delay_us(50);  // Wait for transceiver to switch to transmit mode (typ. 10-30us)
#endif
```

**Section 3: Remove DE pin control in `send_response()` - After TX (lines 518-522)**

Remove this block:
```c
// CRITICAL: Disable RS-485 transmit mode AFTER sending data (if DE pin defined)
#ifdef RS485_DE_PIN
nrf_gpio_pin_clear(RS485_DE_PIN);  // Set DE LOW = receive mode
nrf_delay_us(50);  // Wait for transceiver to switch back to receive mode
#endif
```

#### In `orchestrator_rx_v2.c`:

**Section 1: Remove DE pin configuration in `uart_init()` (lines 285-296)**

Remove this block:
```c
// CRITICAL: Configure RS-485 DE (Driver Enable) pin FIRST (if defined)
// DE pin controls RS-485 transceiver direction:
// - LOW = receive mode (default)
// - HIGH = transmit mode
#ifdef RS485_DE_PIN
nrf_gpio_cfg_output(RS485_DE_PIN);
nrf_gpio_pin_clear(RS485_DE_PIN);  // Set to receive mode (LOW)
nrf_delay_ms(10);  // Wait for transceiver to enter receive mode
test_run_info((unsigned char *)"[DBG] RS-485 DE pin configured (receive mode)");
#else
test_run_info((unsigned char *)"[DBG] RS-485 DE pin not defined - assuming automatic direction control");
#endif
```

**Section 2: Remove DE pin control in `send_response()` - Before TX (lines 471-475)**

Remove this block:
```c
// CRITICAL: Enable RS-485 transmit mode BEFORE sending data (if DE pin defined)
#ifdef RS485_DE_PIN
nrf_gpio_pin_set(RS485_DE_PIN);  // Set DE HIGH = transmit mode
nrf_delay_us(50);  // Wait for transceiver to switch to transmit mode (typ. 10-30us)
#endif
```

**Section 3: Remove DE pin control in `send_response()` - After TX (lines 549-553)**

Remove this block:
```c
// CRITICAL: Disable RS-485 transmit mode AFTER sending data (if DE pin defined)
#ifdef RS485_DE_PIN
nrf_gpio_pin_clear(RS485_DE_PIN);  // Set DE LOW = receive mode
nrf_delay_us(50);  // Wait for transceiver to switch back to receive mode
#endif
```

## Alternative: Keep Code But Ensure RS485_DE_PIN is Undefined

Instead of removing the code, you can ensure `RS485_DE_PIN` is **not defined** in your build configuration. The code is wrapped in `#ifdef RS485_DE_PIN` blocks, so if the symbol is not defined, the code will be skipped during compilation.

### Check in `custom_board.h` or Build Configuration:

Make sure `RS485_DE_PIN` is **NOT** defined:
```c
// DO NOT define this when using Arduino RS485 bridge
// #define RS485_DE_PIN 16  // Leave this commented out or remove it
```

## Why These Changes Are Needed

### Original Design
```
DWM3001CDK → MAX485 → CAT5 Cable → USB-RS485 → PC
           ↑
         DE/RE control by DWM3001CDK
```

### New Design with Arduino Bridge
```
DWM3001CDK → Arduino → MAX485 → CAT5 Cable → USB-RS485 → PC
           ↑         ↑
         TTL Serial  DE/RE control by Arduino
```

**Key Differences:**
1. **DWM3001CDK now uses simple TTL serial** - No DE/RE control needed
2. **Arduino handles RS485 direction control** - Manages DE/RE pins on MAX485
3. **Communication is bidirectional TTL** - Standard UART TX/RX between DWM and Arduino

## Verification

After modifications, verify:

1. **Compilation:** Firmware compiles without errors
2. **UART Pins:** GPIO14 (TX) and GPIO15 (RX) are configured correctly
3. **Baud Rate:** 115200 baud is maintained
4. **No DE/RE Control:** No GPIO pin is toggled during send_response()

## Impact on Existing Commands

**No changes to command protocol are needed.** All commands work identically:
- PNG/PING
- NT/NODE_TYPE
- STRT/START
- STOP
- STAT/GET_STATS
- CFG/SET_CONFIG

The only change is the physical layer - from RS485 to TTL serial between DWM3001CDK and Arduino.

## Example: Modified `send_response()` Function

**Before (with RS485 DE control):**
```c
static void send_response(const char *response)
{
    uint32_t len = strlen(response);
    uint32_t timeout;
    
    // CRITICAL: Enable RS-485 transmit mode BEFORE sending data
    #ifdef RS485_DE_PIN
    nrf_gpio_pin_set(RS485_DE_PIN);
    nrf_delay_us(50);
    #endif
    
    // Send response string
    for (uint32_t i = 0; i < len; i++)
    {
        timeout = 1000;
        while (app_uart_put(response[i]) != NRF_SUCCESS && timeout > 0)
        {
            timeout--;
            Sleep(1);
        }
    }
    
    // Wait for transmission to complete
    nrf_delay_ms(5);
    
    // CRITICAL: Disable RS-485 transmit mode AFTER sending data
    #ifdef RS485_DE_PIN
    nrf_gpio_pin_clear(RS485_DE_PIN);
    nrf_delay_us(50);
    #endif
}
```

**After (TTL serial only):**
```c
static void send_response(const char *response)
{
    uint32_t len = strlen(response);
    uint32_t timeout;
    
    // Send response string
    for (uint32_t i = 0; i < len; i++)
    {
        timeout = 1000;
        while (app_uart_put(response[i]) != NRF_SUCCESS && timeout > 0)
        {
            timeout--;
            Sleep(1);
        }
    }
    
    // Wait for transmission to complete
    nrf_delay_ms(5);
}
```

## Testing After Modifications

1. **Compile Modified Firmware:**
   ```bash
   make clean
   make
   ```

2. **Flash to DWM3001CDK:**
   - Use J-Link or appropriate programmer
   - Flash TX firmware to one board, RX firmware to another

3. **Connect to Arduino:**
   - GPIO14 (TX) → Arduino D8 (RX)
   - GPIO15 (RX) → Arduino D9 (TX)
   - GND → Arduino GND

4. **Test Communication:**
   - Power on Arduino (will perform handshake)
   - Send PNG command via RS485
   - Verify OK response

5. **Test Full Workflow:**
   - Send STRT command to both nodes
   - Verify UWB transmission begins
   - Send STAT command to check statistics
   - Send STOP command

## Backward Compatibility

If you want to maintain backward compatibility (use DWM3001CDK with or without Arduino), keep the `#ifdef RS485_DE_PIN` blocks and control the behavior via build configuration:

**Build without Arduino (direct RS485):**
```c
#define RS485_DE_PIN 16  // Define in custom_board.h or Makefile
```

**Build with Arduino (TTL serial):**
```c
// Do NOT define RS485_DE_PIN
```

This allows the same source code to work in both configurations.

## Summary

**Recommended Approach:** Remove RS485_DE_PIN code blocks (cleaner, simpler)

**Alternative Approach:** Keep code but ensure RS485_DE_PIN is undefined (maintains flexibility)

**Result:** DWM3001CDK communicates via simple TTL serial with Arduino, which handles all RS485 direction control.

## Related Files

- Original TX firmware: `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c`
- Original RX firmware: `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c`
- Arduino bridge firmware: `Arduino/RS485_Bridge/RS485_Bridge.ino`
- Board configuration: Check `Src/platform/custom_board.h` or build configuration

## Questions?

If you encounter issues:
1. Verify RS485_DE_PIN is not defined
2. Check UART pin configuration (GPIO14, GPIO15)
3. Confirm baud rate is 115200
4. Test with simple PING command
