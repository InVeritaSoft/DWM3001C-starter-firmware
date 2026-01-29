# Command Validation Checklist

## Purpose
This document ensures all commands sent by the orchestrator are properly handled by firmware and won't cause 500 errors.

## Commands Sent by Orchestrator

### 1. PNG (Ping)
- **Orchestrator sends**: `PNG`
- **Expected response**: `OK` (or `OK PING`)
- **Firmware handlers**:
  - ✅ `ex_22_orchestrator_v2`: `strcmp(cmd_upper, "PNG") == 0 || strcmp(cmd_upper, "PING") == 0`
  - ✅ `ex_24_tcp_orchestrator`: `strcmp(cmd_upper, "PNG") == 0 || strcmp(cmd_upper, "PING") == 0`
- **Response format**: `send_response("OK")`
- **Orchestrator check**: `response.startsWith("OK")`
- **Status**: ✅ VALIDATED

### 2. NODE_TYPE
- **Orchestrator sends**: `NODE_TYPE`
- **Expected response**: `OK NODE_TYPE=TX_V2` or `OK NODE_TYPE=RX_V2`
- **Firmware handlers**:
  - ✅ `ex_22_orchestrator_v2`: `strncmp(cmd_upper, "NODE_TYPE", 9) == 0`
  - ✅ `ex_24_tcp_orchestrator`: `strncmp(cmd_upper, "NODE_TYPE", 9) == 0`
- **Response format**: `send_response("OK NODE_TYPE=TX_V2")` or `send_response("OK NODE_TYPE=RX_V2")`
- **Orchestrator check**: `response.startsWith("OK NODE_TYPE=")`
- **Status**: ✅ VALIDATED

### 3. CFG (Configure)
- **Orchestrator sends**: 
  - `CFG` (no parameters - uses defaults)
  - `CFG ch=5 rate=6m8 pl=128 len=64 boost=5 rate_hz=100` (with parameters)
- **Expected response**: `OK CONFIG`
- **Firmware handlers**:
  - ✅ `ex_22_orchestrator_v2 TX`: 
    - `strcmp(cmd_upper, "CFG") == 0` → calls `parse_set_config("")`
    - `strncmp(cmd_upper, "CFG ", 4) == 0` → calls `parse_set_config(params)`
  - ✅ `ex_22_orchestrator_v2 RX`: Same as TX
  - ✅ `ex_24_tcp_orchestrator TX`: Same as ex_22
  - ✅ `ex_24_tcp_orchestrator RX`: Same as ex_22
- **Response format**: `send_response("OK CONFIG")` (sent immediately at start of `parse_set_config`)
- **Orchestrator check**: `response.startsWith("OK CONFIG")`
- **Status**: ✅ VALIDATED (Fixed in this session)

### 4. STRT/START (Start Test)
- **Orchestrator sends**: `STRT`
- **Expected response**: `OK START` or `ERR NOT_CONFIGURED` or `ERR TIMER_NOT_INIT`
- **Firmware handlers**:
  - ✅ `ex_22_orchestrator_v2 TX`: `strcmp(cmd_upper, "STRT") == 0 || strcmp(cmd_upper, "START_TEST") == 0 || strcmp(cmd_upper, "START") == 0`
  - ✅ `ex_22_orchestrator_v2 RX`: Same as TX
  - ✅ `ex_24_tcp_orchestrator TX`: Same as ex_22
  - ✅ `ex_24_tcp_orchestrator RX`: Same as ex_22
- **Response format**: `send_response("OK START")` or `send_response("ERR NOT_CONFIGURED")`
- **Orchestrator check**: `response.startsWith("OK START")`
- **Status**: ✅ VALIDATED (Fixed in this session)

### 5. STOP (Stop Test)
- **Orchestrator sends**: `STOP`
- **Expected response**: `OK STOP`
- **Firmware handlers**:
  - ✅ `ex_22_orchestrator_v2 TX`: `strcmp(cmd_upper, "STOP") == 0 || strcmp(cmd_upper, "STOP_TEST") == 0`
  - ✅ `ex_22_orchestrator_v2 RX`: Same as TX
  - ✅ `ex_24_tcp_orchestrator TX`: Same as ex_22
  - ✅ `ex_24_tcp_orchestrator RX`: Same as ex_22
- **Response format**: `send_response("OK STOP")`
- **Orchestrator check**: `response.startsWith("OK STOP")`
- **Status**: ✅ VALIDATED

### 6. STAT/STATS (Get Statistics)
- **Orchestrator sends**: `STAT`
- **Expected response**: `OK STATS ...` (with statistics data)
- **Firmware handlers**:
  - ✅ `ex_22_orchestrator_v2 TX`: `strcmp(cmd_upper, "STAT") == 0 || strcmp(cmd_upper, "GET_STATS") == 0 || strcmp(cmd_upper, "STATS") == 0`
  - ✅ `ex_22_orchestrator_v2 RX`: Same as TX
  - ✅ `ex_24_tcp_orchestrator TX`: Same as ex_22
  - ✅ `ex_24_tcp_orchestrator RX`: Same as ex_22
- **Response format**: `send_response("OK STATS sent=... attempted=... errors=... timeouts=... last_err=... frame_dur=...")`
- **Orchestrator check**: `response.startsWith("OK STATS")`
- **Status**: ✅ VALIDATED (Fixed in this session)

### 7. RST (Reset Statistics)
- **Orchestrator sends**: `RST`
- **Expected response**: `OK`
- **Firmware handlers**:
  - ✅ `ex_22_orchestrator_v2 TX`: `strcmp(cmd_upper, "RST") == 0 || strcmp(cmd_upper, "RESET_STATS") == 0`
  - ✅ `ex_22_orchestrator_v2 RX`: Same as TX
  - ✅ `ex_24_tcp_orchestrator TX`: Same as ex_22
  - ✅ `ex_24_tcp_orchestrator RX`: Same as ex_22
- **Response format**: `send_response("OK")`
- **Orchestrator check**: `response.startsWith("OK")`
- **Status**: ✅ VALIDATED

## Error Handling

### Firmware Error Responses
- `ERR UNKNOWN_CMD` - Command not recognized
- `ERR NOT_CONFIGURED` - Node not configured before START
- `ERR TIMER_NOT_INIT` - Timer not initialized (TX only)
- `ERR START_FAILED` - Failed to start test

### Orchestrator Error Handling
- ✅ All API routes wrapped in `try-catch` blocks
- ✅ Errors return `res.status(500).json({ error: error.message })`
- ✅ Command timeouts return detailed error messages
- ✅ Special handling for `ERR NOT_CONFIGURED` (returns 400, not 500)

## Potential 500 Error Causes

### 1. Command Timeout
- **Cause**: Firmware doesn't respond within timeout period
- **Prevention**: 
  - ✅ Firmware sends response immediately (before long operations)
  - ✅ Timeouts are reasonable (5-20 seconds depending on command)
  - ✅ Error message includes troubleshooting steps

### 2. Unexpected Response Format
- **Cause**: Firmware returns response that doesn't match expected format
- **Prevention**:
  - ✅ All command handlers verified to send correct format
  - ✅ Orchestrator uses `startsWith()` checks (more forgiving than exact match)

### 3. Null/Undefined Response
- **Cause**: `sendCommand()` returns null or undefined
- **Prevention**:
  - ✅ `handleResponse()` always resolves/rejects Promise
  - ✅ Response parsing validates data before processing

### 4. Serial Port Errors
- **Cause**: Serial port write/drain fails
- **Prevention**:
  - ✅ Write errors caught and Promise rejected
  - ✅ Drain errors caught and Promise rejected
  - ✅ Auto-reconnect logic attempts to reopen port

### 5. Unknown Command
- **Cause**: Firmware receives command it doesn't recognize
- **Prevention**:
  - ✅ All orchestrator commands verified to have firmware handlers
  - ✅ Firmware returns `ERR UNKNOWN_CMD` (not a 500 error)

## Verification Steps

1. ✅ All command handlers exist in both firmware versions
2. ✅ Command aliases supported (CFG/CONFIG/SET_CONFIG, STRT/START/START_TEST, etc.)
3. ✅ Response formats match orchestrator expectations
4. ✅ Error responses properly formatted
5. ✅ Timeouts are reasonable for each command type
6. ✅ Firmware sends responses immediately (before long operations)

## Testing Recommendations

1. **Test each command individually**:
   - Send PNG → expect `OK`
   - Send NODE_TYPE → expect `OK NODE_TYPE=...`
   - Send CFG → expect `OK CONFIG`
   - Send CFG with params → expect `OK CONFIG`
   - Send STRT → expect `OK START` or `ERR NOT_CONFIGURED`
   - Send STOP → expect `OK STOP`
   - Send STAT → expect `OK STATS ...`
   - Send RST → expect `OK`

2. **Test error cases**:
   - Send START without CFG → expect `ERR NOT_CONFIGURED` (400, not 500)
   - Send invalid command → expect `ERR UNKNOWN_CMD` (handled gracefully)
   - Disconnect serial port → expect timeout error (500, but with helpful message)

3. **Test edge cases**:
   - Send CFG with empty string → should use defaults
   - Send CFG with invalid params → should still return `OK CONFIG` (firmware uses defaults)
   - Send commands rapidly → should handle FIFO correctly

## Conclusion

All commands are now properly validated and should not cause 500 errors unless:
1. Hardware failure (serial port disconnected)
2. Firmware not running (timeout)
3. Unexpected firmware bug (should be rare)

The fixes applied ensure:
- ✅ All command variations are handled
- ✅ Responses match expected formats
- ✅ Error handling is comprehensive
- ✅ Timeouts are reasonable
