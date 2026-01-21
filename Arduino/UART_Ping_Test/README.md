# Arduino UART Ping Test

## Overview

Simple Arduino firmware that communicates with DWM3001CDK:
- Receives "Hello World" from DWM3001CDK
- Responds with "PING" when "Hello World" is detected

## Pin Configuration

- **D8 (RX)**: Receives data from DWM3001CDK TX (GPIO 27, J10 Pin 19, GPIO27_PIN19_TX)
- **D9 (TX)**: Sends data to DWM3001CDK RX (GPIO 15, J10 Pin 10)
- **D13 (LED)**: Built-in LED blinks when "Hello World" is received

## Wiring

```
Arduino          DWM3001CDK J10
──────────      ─────────────────
D8 (RX)      ←  Pin 19 (TX/GPIO27, GPIO27_PIN19_TX)
D9 (TX)      →  Pin 10 (RX/GPIO15)
GND          →  Pin 6 (GND)
```

## Communication

- **Baud Rate**: 115200
- **Receives**: `"Hello World\r\n"`
- **Sends**: `"PING"`

## Usage

1. Upload this sketch to your Arduino
2. Connect Arduino D8/D9 to DWM3001CDK J10 pins as shown above
3. Open Serial Monitor (115200 baud) to see debug messages
4. The built-in LED will blink when "Hello World" is received

## Expected Behavior

1. Built-in LED blinks 3 times on startup
2. Serial Monitor shows: "Arduino UART Ping Test - Ready"
3. When "Hello World" is received:
   - Built-in LED blinks briefly
   - Serial Monitor shows: "Received: Hello World"
   - Arduino sends "PING"
   - Serial Monitor shows: "Sent: PING"
