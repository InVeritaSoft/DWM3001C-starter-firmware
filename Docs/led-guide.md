# LED Guide for DWM3001CDK

## Physical LED Locations

| LED | Color | Pin | BSP_LED | Function |
|-----|-------|-----|---------|----------|
| **D9** | **Red** | P0.04 | LED_0 | Startup/Error indicator |
| **D10** | **Orange** | P0.05 | LED_1 | RS-485 RX (command received) |
| **D11** | **Green** | P0.22 | LED_2 | RS-485 TX (response sent) |
| **D12** | **Blue** | P0.14 | LED_3 | **DISABLED** (conflicts with UART TX) |
| **D13** | **Yellow/Orange** | DW3000 internal | N/A | DW3000 chip status |

## Startup Sequence (Normal)

1. **D9 (Red)**: Blinks 3 times → Firmware starting
2. **D10 (Orange)**: Blinks 2 times → UART initialized
3. **D13 (Yellow)**: Slow blink → DW3000 initialized
4. **D9 (Red)**: Should turn OFF after startup completes

## During RS-485 Communication

- **D10 (Orange)**: Blinks briefly (50ms) when command received
- **D11 (Green)**: Blinks briefly when response sent

## Your Observation

- "3 times blue D10" → This is **D9 (Red)** blinking 3 times on startup ✓
- "yellow D13 blinking" → **D13** slow blink = DW3000 initialized ✓
- "D9 Always green" → This might be **D11 (Green)** staying on, or color confusion

## What to Watch For

When sending commands from RPi5:
1. **D10 (Orange)** should blink → Command received by firmware
2. **D11 (Green)** should blink → Response sent by firmware

If D10 doesn't blink → Commands not reaching firmware
If D11 doesn't blink → Responses not being sent

