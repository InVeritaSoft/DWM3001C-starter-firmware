# Arduino Command Test Bridge

## Overview

This Arduino firmware acts as a bridge between your Serial Monitor and the DWM3001CDK orchestrator_v2 firmware. It forwards commands from Serial Monitor to DWM3001CDK and displays responses.

## Features

- **Bidirectional Communication**: Commands from Serial Monitor → DWM3001CDK, Responses from DWM3001CDK → Serial Monitor
- **Node Identification**: Automatically detects Node A (TX) or Node B (RX) via A0 pin
- **Command Support**: All orchestrator_v2 commands (PING, START, STOP, STATS, CONFIG, NODE_TYPE, RESET_STATS)
- **Timeout Handling**: Detects when DWM3001CDK doesn't respond
- **Visual Feedback**: Built-in LED blinks on command send/receive

## Pin Configuration

| Arduino Pin | DWM3001CDK Pin | Description |
|-------------|----------------|-------------|
| **D8** | GPIO27 (J10 Pin 19, GPIO27_PIN19_TX) | Arduino RX ← DWM TX |
| **D9** | GPIO15 (J10 Pin 10) | Arduino TX → DWM RX |
| **A0** | GND (for Node A) or Floating/5V (for Node B) | Node identification |
| **D13** | Built-in LED | Status indicator |
| **GND** | GND | Common ground |

## Node Identification

The firmware automatically detects which node it's connected to:

- **Node A (TX)**: Connect A0 to GND
- **Node B (RX)**: Leave A0 floating or connect to 5V

The node type is displayed in the startup message.

## Wiring

```
DWM3001CDK J10          Arduino
─────────────────      ──────────
Pin 19 (TX/GPIO27)  →  D8 (RX)  [GPIO27_PIN19_TX]
Pin 10 (RX/GPIO15)  ←  D9 (TX)
Pin 6  (GND)        →  GND

For Node A: A0 → GND
For Node B: A0 → Floating or 5V
```

## Communication Protocol

- **Baud Rate**: 115200 (both Serial Monitor and DWM3001CDK)
- **Command Format**: Text commands with `\r\n` delimiter
- **Response Format**: Text responses with `\r\n` delimiter
- **Timeout**: 3 seconds (if no response received)

## Supported Commands

### Basic Commands

| Command | Short Form | Description |
|---------|-----------|-------------|
| `PING` | `PNG` | Test connectivity |
| `NODE_TYPE` | - | Get node type (TX_V2 or RX_V2) |
| `START` | `STRT` or `START_TEST` | Start sending/receiving packets |
| `STOP` | `STOP_TEST` | Stop sending/receiving packets |
| `STATS` | `STAT` or `GET_STATS` | Get statistics |
| `RESET_STATS` | `RST` | Reset statistics |

### Configuration Command

```
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
```

or

```
SET_CONFIG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
```

**Parameters:**
- `ch`: Channel (5 or 9)
- `rate`: Data rate (`850k` or `6m8`)
- `pl`: Preamble length (64, 128, 256, 512, 1024)
- `len`: Payload length (bytes)
- `pwr_ref`: Reference TX power (hex, e.g., `0x36363636`)
- `boost`: Power boost (0.1dB steps, typically 0-30)
- `rate_hz`: Packet rate (packets per second)

## Usage

1. **Upload Firmware**: Upload `Command_Test.ino` to your Arduino
2. **Open Serial Monitor**: Set baud rate to 115200
3. **Identify Node**: Check startup message to confirm node type
4. **Send Commands**: Type commands in Serial Monitor and press Enter

## Example Session

```
========================================
DWM3001CDK Command Test Bridge
Node Type: A (TX)
========================================
Ready! Type commands and press Enter:
  - PING
  - NODE_TYPE
  - START
  - STOP
  - STATS
  - CONFIG <params>
  - RESET_STATS
========================================

[SEND] PING
[RECV] OK

[SEND] NODE_TYPE
[RECV] OK NODE_TYPE=TX_V2

[SEND] CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
[RECV] OK CONFIG

[SEND] START
[RECV] OK START

[SEND] STATS
[RECV] OK STATS sent=1234 attempted=1234 errors=0 timeouts=0 last_err=0 frame_dur=256

[SEND] STOP
[RECV] OK STOP
```

## Expected Behavior

### Node A (TX)
- `START` command begins transmitting UWB packets
- `STATS` shows `sent`, `attempted`, `errors`, `timeouts`
- `STOP` command stops transmission

### Node B (RX)
- `START` command begins receiving UWB packets
- `STATS` shows `rx`, `lost`, `crc_err`, `phy_err`, `timeout`, `overrun`, RSSI stats
- `STOP` command stops reception

## Troubleshooting

### No Response from DWM3001CDK
- Check wiring (D8/D9 connections)
- Verify DWM3001CDK is powered and running orchestrator_v2 firmware
- Check baud rate (115200)
- Try `PING` command first

### Wrong Node Type Detected
- For Node A: Ensure A0 is connected to GND
- For Node B: Ensure A0 is floating or connected to 5V

### Commands Not Working
- Ensure DWM3001CDK is configured before sending `START`
- Use `CFG` command first, then `START`
- Check that responses end with `\r\n`

## Notes

- Commands are case-insensitive (automatically converted to uppercase by DWM3001CDK)
- Responses may take up to 3 seconds (timeout)
- LED blinks on command send and response receive
- Unsolicited messages from DWM3001CDK are marked with `[UNSOLICITED]`
