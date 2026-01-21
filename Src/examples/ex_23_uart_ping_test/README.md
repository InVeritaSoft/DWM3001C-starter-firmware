# UART Ping Test Example

## Overview

This example demonstrates simple UART communication between DWM3001CDK and Arduino:
- DWM3001CDK sends "Hello World" every 2 seconds
- Arduino receives "Hello World" and responds with "PING"
- DWM3001CDK lights blue LED when "PING" is received

## Pin Configuration

### DWM3001CDK (J10 Connector)
- **TX**: GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX)
- **RX**: GPIO 15 (P0.15) - J10 Pin 10 (left side)
- **Blue LED**: BSP_LED_1 (D10) - Lights when SW2 pressed
- **Orange LED**: BSP_LED_3 (D12) - Lights when "PING" received

### Arduino
- **RX (D8)**: Connected to DWM3001CDK TX (GPIO 27, J10 Pin 19)
- **TX (D9)**: Connected to DWM3001CDK RX (GPIO 15, J10 Pin 10)
- **LED**: Built-in LED (D13) blinks when "Hello World" received

## Wiring

```
DWM3001CDK J10          Arduino
─────────────────      ──────────
Pin 19 (TX/GPIO27)  →  D8 (RX)  [GPIO27_PIN19_TX]
Pin 10 (RX/GPIO15)  ←  D9 (TX)
Pin 6  (GND)        →  GND
```

## Communication Protocol

- **Baud Rate**: 115200
- **DWM3001CDK sends**: `"Hello World\r\n"` (every 2 seconds)
- **Arduino responds**: `"PING"` (when "Hello World" is detected)

## Building and Flashing

### DWM3001CDK

1. Enable the test in `Src/example_selection.h`:
   ```c
   #define TEST_UART_PING
   ```

2. Uncomment the function call in `Src/main.c`:
   ```c
   extern int uart_ping_test(void); uart_ping_test();
   ```

3. Build and flash using your normal build process.

### Arduino

1. Open `Arduino/UART_Ping_Test/UART_Ping_Test.ino` in Arduino IDE
2. Select your Arduino board and port
3. Upload the sketch

## Expected Behavior

### DWM3001CDK
1. Red LED blinks 3 times on startup
2. Blue LED (D10) briefly turns on (200ms) when SW2 is pressed → sends "Hello World"
3. Orange LED (D12) turns on for 1 second when "PING" is received

### Arduino
1. Built-in LED blinks 3 times on startup
2. Built-in LED blinks briefly when "Hello World" is received
3. Sends "PING" response automatically

## Debugging

- Check RTT logs on DWM3001CDK for debug messages
- Use Arduino Serial Monitor (115200 baud) to see Arduino debug output
- Verify wiring connections match the pin configuration above
