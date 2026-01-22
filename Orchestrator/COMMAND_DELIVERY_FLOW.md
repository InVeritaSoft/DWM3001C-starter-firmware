# Command Delivery and Response Flow

## Overview

This document describes how commands flow from the Node.js Orchestrator through RS485_Bridge to DWM3001CDK firmware and how responses flow back.

## Command Flow: Orchestrator → RS485_Bridge → DWM3001CDK

### 1. Orchestrator (Node.js) - Command Sending

**File**: `Orchestrator/src/orchestrator/rs485Comm.js`

- **Method**: `sendCommand(command, timeout)`
- **Format**: `command + "\r\n"` (e.g., `"PNG\r\n"`)
- **Process**:
  1. Auto-opens serial port if not open
  2. Creates command ID for tracking
  3. Formats command with `\r\n` terminator
  4. Writes to serial port
  5. Drains port to ensure data is sent
  6. Waits for response with timeout
  7. Matches response to command using FIFO (oldest command first)

**Supported Commands**:
- `PNG` - Ping (connectivity check)
- `NODE_TYPE` - Get firmware node type
- `CFG` - Set configuration (e.g., `CFG ch=5 rate=6m8 pl=128 len=64`)
- `STRT` - Start test
- `STOP` - Stop test
- `STAT` - Get statistics
- `RST` - Reset statistics

### 2. RS485_Bridge (Arduino) - Command Forwarding

**File**: `Arduino/RS485_Bridge/RS485_Bridge.ino`

- **Function**: `rs485_receive_command()` - Receives from orchestrator
- **Function**: `dwm_send_command()` - Forwards to DWM3001CDK
- **Process**:
  1. Receives command from RS485 serial (orchestrator)
  2. Parses command (reads until `\r` or `\n`)
  3. **Special handling**: `CLOCK_SYNC` command is handled locally (not forwarded)
  4. Switches SoftwareSerial listener to DWM
  5. Clears DWM buffer
  6. Forwards command EXACTLY as received (transparent bridge)
  7. Adds `\r\n` terminator
  8. Waits for response from DWM
  9. Forwards response back to orchestrator via RS485

**Key Features**:
- Transparent bridge: Commands forwarded without modification
- Listener management: Switches between RS485 and DWM serial ports
- Buffer clearing: Clears DWM buffer before sending to avoid stale data
- Response forwarding: Forwards DWM responses back to orchestrator

### 3. DWM3001CDK Firmware - Command Processing

**Files**: 
- `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c`
- `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c`
- `Src/examples/ex_24_tcp_orchestrator/tcp_orchestrator_tx.c`
- `Src/examples/ex_24_tcp_orchestrator/tcp_orchestrator_rx.c`

- **Process**:
  1. Receives command via UART (GPIO15/RXD0)
  2. Parses command
  3. Executes command handler
  4. Sends response via UART (GPIO27/TXD0)
  5. Format: `OK <data>\r\n` or `ERR <type>\r\n`

## Response Flow: DWM3001CDK → RS485_Bridge → Orchestrator

### 1. DWM3001CDK Firmware - Response Sending

- **Format**: `OK <data>\r\n` or `ERR <type>\r\n`
- **Examples**:
  - `OK PING\r\n`
  - `OK NODE_TYPE=TX_V2\r\n`
  - `OK CONFIG\r\n`
  - `OK START\r\n`
  - `OK STATS total_sent=100 ...\r\n`
  - `ERR INVALID_COMMAND\r\n`

### 2. RS485_Bridge - Response Forwarding

- **Function**: `dwm_receive_response()` - Receives from DWM
- **Function**: `rs485_send_response()` - Forwards to orchestrator
- **Process**:
  1. Waits for response from DWM (reads until `\r` or `\n`)
  2. Sets RS485 to TX mode (DE=HIGH, RE=HIGH)
  3. Sends response to orchestrator
  4. Adds `\r\n` terminator if not present
  5. Sets RS485 back to RX mode (DE=LOW, RE=LOW)

### 3. Orchestrator - Response Handling

- **Method**: `handleResponse(data)`
- **Process**:
  1. Receives response via serial port parser
  2. Filters out debug messages (e.g., `[UART]`, `[CMD]`, etc.)
  3. Matches response to oldest pending command (FIFO)
  4. Resolves or rejects Promise based on `OK`/`ERR` prefix
  5. Emits unsolicited data events for unmatched responses

## Command/Response Matching

### FIFO Matching Strategy

The orchestrator uses **FIFO (First In, First Out)** matching:
- Commands are assigned sequential IDs
- Responses are matched to the **oldest pending command**
- This ensures responses match commands in order
- Prevents race conditions from out-of-order responses

### Response Format Requirements

- Must start with `OK` or `ERR` (case-insensitive)
- Must end with `\r\n` or `\n`
- Debug messages are filtered out (e.g., `[UART]`, `[CMD]`)

## Error Handling

### Timeout Handling

- **Orchestrator**: Default timeout is 1000ms (configurable)
- **RS485_Bridge**: DWM response timeout is 5000ms
- **On timeout**: Command Promise is rejected with error message

### Error Scenarios

1. **Command timeout**: No response received within timeout period
2. **Invalid command**: Firmware returns `ERR INVALID_COMMAND`
3. **Serial port errors**: Port not open, write errors, etc.
4. **Buffer overflow**: Command/response too long for buffer
5. **Listener switching**: SoftwareSerial listener conflicts

## Best Practices

### Command Formatting

- Use short command codes (`PNG` instead of `PING`) for reliability
- Always include parameters in correct format (e.g., `CFG ch=5 rate=6m8`)
- Commands are case-sensitive (use uppercase)

### Response Handling

- Always check for `OK` or `ERR` prefix
- Parse response data after prefix (e.g., `OK STATS total_sent=100`)
- Handle timeouts gracefully with retry logic if needed

### Debugging

- Enable `DEBUG_RS485` environment variable for detailed logging
- Check serial port logs for command/response flow
- Monitor RS485_Bridge serial output for forwarding status
- Check DWM3001CDK UART output for firmware processing

## Testing

### Manual Testing

1. **Test PING**:
   ```bash
   # Send PNG command via serial terminal
   echo -e "PNG\r\n" > /dev/ttyUSB0
   # Expected: OK PING
   ```

2. **Test via Orchestrator**:
   ```bash
   npm run test-serial -- COM17
   ```

3. **Monitor RS485_Bridge**:
   - Open Arduino Serial Monitor (115200 baud)
   - Watch for command forwarding logs
   - Check for response forwarding

### Automated Testing

- Use `testRunner.js` for automated test sequences
- Commands are sent via `nodeController.js`
- Responses are logged and validated

## Troubleshooting

### Commands Not Received

1. Check RS485 wiring (A+/B-, GND, termination resistors)
2. Verify serial port is correct
3. Check baud rate matches (57600 for RS485_Bridge, 115200 for DWM)
4. Verify MAX485 transceiver is powered (5V)

### Responses Not Received

1. Check DWM3001CDK firmware is running
2. Verify UART wiring (GPIO15/RX, GPIO27/TX)
3. Check SoftwareSerial listener is on correct port
4. Verify response format matches expected format

### Timeout Errors

1. Increase timeout value if needed
2. Check for buffer overflow (command too long)
3. Verify firmware is processing commands
4. Check for listener switching conflicts
