# Arduino RS485 Bridge Firmware

## Overview

This Arduino Uno firmware acts as a transparent communication bridge between an RS485 orchestrator (PC or Raspberry Pi 5) and a DWM3001CDK UWB module. The Arduino handles RS485 direction control and forwards commands bidirectionally without modification.

## Architecture

```
Orchestrator (PC/RPi5)
    ↕ USB
USB-RS485 Adapter
    ↕ CAT5/CAT6 (A+, B-)
MAX485 Converter
    ↕ TTL Serial (D2, D3, D10, D11)
Arduino Uno
    ↕ TTL Serial (D8, D9)
DWM3001CDK Board
    ↕ UWB Radio
UWB Communication
```

## Hardware Requirements

### Components
- 1x Arduino Uno (or compatible)
- 1x MAX485 RS-485 to TTL Converter Module
- 1x DWM3001CDK UWB Development Board
- 4x LEDs (optional, for status indicators)
- 4x 220Ω Resistors (for LEDs)
- Jumper wires
- CAT5/CAT6 cable for RS485 connection

### Wiring Diagram

#### MAX485 → Arduino Uno

| MAX485 Pin | Arduino Pin | Description |
|------------|-------------|-------------|
| VCC        | 5V          | Power supply |
| GND        | GND         | Ground |
| DI         | D11         | Data Input (Transmit to MAX485) |
| RO         | D10         | Receiver Output (Receive from MAX485) |
| DE         | D3          | Driver Enable (HIGH = transmit) |
| RE         | D2          | Receiver Enable (LOW = receive) |
| A          | A+ on CAT5  | RS485 A+ (Orange wire) |
| B          | B- on CAT5  | RS485 B- (White-Orange wire) |

#### Arduino Uno → DWM3001CDK

| Arduino Pin | DWM3001CDK Pin | Description |
|-------------|----------------|-------------|
| D9          | GPIO14 (J10-10, RXD0) | Arduino TX → DWM RX |
| D8          | GPIO15 (J10-8, TXD0)  | Arduino RX ← DWM TX |
| GND         | GND            | Common ground |

**Important:** Do NOT connect 5V from Arduino to DWM3001CDK. The DWM3001CDK is powered separately via USB or its own power supply.

#### LED Indicators (Optional)

| LED Purpose | Arduino Pin | LED Connection |
|-------------|-------------|----------------|
| RX Activity | D4          | LED Anode → D4, LED Cathode → 220Ω resistor → GND |
| TX Activity | D5          | LED Anode → D5, LED Cathode → 220Ω resistor → GND |
| Error       | D6          | LED Anode → D6, LED Cathode → 220Ω resistor → GND |
| Heartbeat   | D13         | Built-in LED (no external wiring needed) |

#### Complete Wiring Summary

```
MAX485 Module:
  VCC  → Arduino 5V
  GND  → Arduino GND
  DI   → Arduino D11
  RO   → Arduino D10
  DE   → Arduino D3
  RE   → Arduino D2
  A    → CAT5 Orange wire (A+)
  B    → CAT5 White-Orange wire (B-)

Arduino Uno:
  D2   → MAX485 RE
  D3   → MAX485 DE
  D10  → MAX485 RO
  D11  → MAX485 DI
  D8   → DWM3001CDK GPIO15 (TXD0, J10 Pin 8)
  D9   → DWM3001CDK GPIO14 (RXD0, J10 Pin 10)
  D4   → RX LED (+ 220Ω resistor)
  D5   → TX LED (+ 220Ω resistor)
  D6   → Error LED (+ 220Ω resistor)
  D13  → Built-in Heartbeat LED
  GND  → MAX485 GND, DWM3001CDK GND, LED resistors

DWM3001CDK:
  GPIO15 (J10 Pin 8, TXD0)  → Arduino D8 (RX)
  GPIO14 (J10 Pin 10, RXD0) → Arduino D9 (TX)
  GND                       → Arduino GND
```

## Installation

### 1. Install Arduino IDE

Download and install the Arduino IDE from [arduino.cc](https://www.arduino.cc/en/software)

### 2. Open Firmware

1. Launch Arduino IDE
2. Open `RS485_Bridge.ino` from this directory
3. Select **Tools → Board → Arduino Uno**
4. Select **Tools → Port** and choose the correct COM port for your Arduino

### 3. Upload Firmware

1. Click the **Upload** button (right arrow icon)
2. Wait for compilation and upload to complete
3. The Arduino will automatically reset after upload

### 4. Verify Operation

After upload, observe the LED behavior:

**Normal Startup Sequence:**
1. All LEDs turn ON for 500ms (startup indicator)
2. All LEDs turn OFF
3. Arduino attempts handshake with DWM3001CDK
4. If successful: Heartbeat LED blinks 3 times rapidly
5. If failed: Error LED blinks rapidly (10 times) then stays ON

**During Normal Operation:**
- **Heartbeat LED (D13)**: Blinks at 1Hz (ON for 500ms, OFF for 500ms) - indicates Arduino is running
- **RX LED (D4)**: Blinks briefly (50ms) when command received from orchestrator
- **TX LED (D5)**: Blinks briefly (50ms) when response sent to orchestrator
- **Error LED (D6)**: Solid ON indicates communication error

## Operation

### Supported Commands

The Arduino transparently forwards all commands to the DWM3001CDK. Supported commands include:

| Command | Alias | Description | Response |
|---------|-------|-------------|----------|
| PNG     | PING  | Connectivity check | OK |
| NT      | NODE_TYPE | Query node type | OK TX_V2 or OK RX_V2 |
| STRT    | START | Start UWB test | OK START |
| STOP    | -     | Stop UWB test | OK STOP |
| STAT    | GET_STATS | Get statistics | OK STATS <data> |
| CFG ... | SET_CONFIG | Configure UWB parameters | OK CONFIG |

### Command Format

Commands are sent via RS485 as ASCII text terminated by `\r` (carriage return) or `\n` (newline).

**Example:**
```
PNG\r\n
```

### Response Format

Responses follow the format:
- Success: `OK [data]\r\n`
- Error: `ERR_<ERROR_TYPE>\r\n`

**Examples:**
```
OK\r\n
OK TX_V2\r\n
OK START\r\n
ERR_DWM_NO_RESPONSE\r\n
```

### Error Codes

| Error Code | Description |
|------------|-------------|
| ERR_HANDSHAKE_FAILED | Initial handshake with DWM3001CDK failed |
| ERR_DWM_NO_RESPONSE | DWM3001CDK did not respond to command |
| ERR_INVALID_DATA | Invalid or corrupted data received |

## Troubleshooting

### Handshake Fails on Startup (Error LED stays ON)

**Symptoms:** Error LED blinks rapidly then stays ON. Heartbeat may or may not blink.

**Possible Causes:**
1. DWM3001CDK not powered on
2. Incorrect wiring between Arduino and DWM3001CDK
3. DWM3001CDK firmware not loaded or wrong version
4. Baud rate mismatch

**Solutions:**
1. Verify DWM3001CDK is powered and LEDs are blinking
2. Double-check D8↔GPIO14 and D9↔GPIO15 connections
3. Flash DWM3001CDK with `orchestrator_tx_v2.c` or `orchestrator_rx_v2.c` firmware
4. Verify both firmwares use 115200 baud rate

### No Response from Orchestrator Commands

**Symptoms:** Commands sent via RS485 but no response received

**Possible Causes:**
1. RS485 wiring incorrect
2. A+/B- reversed
3. MAX485 not powered
4. Wrong CAT5 wires used

**Solutions:**
1. Verify MAX485 VCC→5V and GND→GND
2. Check A+ connects to Orange wire, B- to White-Orange wire
3. Swap A+ and B- if needed
4. Use twisted pair wires for A+/B- (Orange and White-Orange in CAT5)

### Commands Work But Random Errors

**Symptoms:** Intermittent `ERR_DWM_NO_RESPONSE` errors

**Possible Causes:**
1. SoftwareSerial limitations at 115200 baud
2. Electrical noise on serial lines
3. Ground loop issues

**Solutions:**
1. Reduce baud rate to 57600 (modify firmware and recompile)
2. Use shorter cables between Arduino and DWM3001CDK
3. Add 0.1µF capacitor between DWM3001CDK VCC and GND near serial pins
4. Ensure all grounds are properly connected

### LEDs Don't Blink During Communication

**Symptoms:** Heartbeat works but RX/TX LEDs never blink

**Possible Causes:**
1. LEDs not connected or wrong polarity
2. Resistors missing or wrong value
3. Commands not reaching Arduino

**Solutions:**
1. Check LED polarity (long leg = anode → Arduino pin)
2. Verify 220Ω resistors are in series with LEDs
3. Test communication with Serial Monitor

## Serial Monitor Debugging

For development and debugging, you can monitor communication via the Arduino IDE Serial Monitor, but note:

- Hardware Serial (D0/D1) is **NOT** used by this firmware
- To debug, add `Serial.begin(115200)` in `setup()` and `Serial.println()` statements in code
- This will allow monitoring via USB while RS485 operates independently

**Example debug addition:**
```cpp
void setup() {
  Serial.begin(115200);  // Add this for debugging
  Serial.println("Arduino RS485 Bridge Starting...");
  // ... rest of setup code
}
```

## Performance Characteristics

- **Baud Rate:** 115200 bps
- **Maximum Cable Length:** 50m (CAT5/CAT6 RS485)
- **Command Latency:** ~10-50ms (depending on DWM3001CDK processing)
- **RS485 Switching Time:** 100µs (DE/RE transceiver switching)
- **Maximum Command Length:** 256 characters
- **DWM Response Timeout:** 5 seconds

## Limitations

1. **No Command Buffering:** Arduino processes one command at a time. The orchestrator must wait for a response before sending the next command.

2. **No Built-in Timeout:** Arduino waits indefinitely for DWM responses. The orchestrator should implement its own timeout logic.

3. **SoftwareSerial at High Baud Rates:** Arduino Uno's SoftwareSerial may have occasional errors at 115200 baud. If reliability issues occur, reduce baud rate to 57600.

4. **Single RS485 Bus:** Only one orchestrator can communicate with the Arduino at a time (half-duplex RS485).

## Technical Notes

### RS485 Half-Duplex Operation

The MAX485 operates in half-duplex mode:
- **Receive Mode (default):** DE=LOW, RE=LOW
- **Transmit Mode:** DE=HIGH, RE=HIGH

The firmware automatically handles mode switching with proper timing delays to prevent data corruption.

### Timing Considerations

- **RS485 Transceiver Switching:** 100µs delay before/after transmission
- **Serial Flush:** Ensures all bytes transmitted before switching modes
- **DWM Stabilization:** 500ms delay on startup for DWM3001CDK initialization

## License

This firmware is part of the INVERITA DWM3001C Test Rig project.

## Version History

- **v1.0** (2026-01-20): Initial release
  - Transparent RS485 bridge functionality
  - Automatic handshake with DWM3001CDK
  - LED status indicators
  - Error handling and reporting

## Support

For issues or questions:
1. Check wiring against diagrams above
2. Verify all power connections
3. Test with Serial Monitor debugging enabled
4. Check DWM3001CDK firmware version compatibility
