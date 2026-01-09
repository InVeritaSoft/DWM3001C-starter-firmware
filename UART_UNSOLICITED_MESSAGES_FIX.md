# UART Unsolicited Messages Fix

## Problem Summary

The orchestrator v2 firmware was sending **unsolicited startup messages** over UART during initialization:
- `OK STARTUP V2`
- `OK DW3000_READY`
- `OK MAIN_LOOP`

These messages were being sent **before** the orchestrator software sent any commands, causing:
1. **RS-485 warnings**: "Received OK/ERR response with no pending command"
2. **Command timeouts**: When the orchestrator sent `PNG` (PING) commands, the firmware didn't respond
3. **Repeated reboots**: The firmware appeared to be rebooting repeatedly (startup messages appeared multiple times)

## Root Causes

### 1. Unsolicited UART Messages
The firmware was broadcasting status messages during initialization without being asked. The orchestrator expects a **command-response** model where:
- Orchestrator sends command (e.g., `PNG\r\n`)
- Firmware responds (e.g., `OK\r\n`)

Unsolicited messages break this model and confuse the command tracking system.

### 2. Double Response in RX Firmware
The RX firmware had a bug where it sent the response twice:
```c
send_response("OK");
send_response("\r\n");  // WRONG - send_response already adds \r\n!
```

This resulted in `OK\r\n\r\n` being sent, which is malformed.

### 3. Excessive Debug Logging in UART Interrupt
The RX firmware was calling `test_run_info()` (which uses `printf()` and SEGGER RTT) **inside the UART interrupt handler** for every byte received. This could cause:
- Interrupt handler to take too long
- Potential crashes or hangs
- Missed UART bytes

## Changes Made

### File: `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c`

#### Change 1: Removed Double Response (Line 677)
**Before:**
```c
send_response("OK");
send_response("\r\n");  // Ensure response is complete
```

**After:**
```c
send_response("OK");
```

#### Change 2: Removed Excessive Debug Logging (Lines 215-270)
**Before:**
```c
// DEBUG: Log every byte received
char debug_msg[32];
snprintf(debug_msg, sizeof(debug_msg), "[RX] byte=0x%02X '%c'", byte, (byte >= 32 && byte < 127) ? byte : '?');
test_run_info((unsigned char *)debug_msg);

// ... more logging in interrupt handler
```

**After:**
```c
// Removed all test_run_info() calls from UART interrupt handler
// Only kept LED blinking for visual feedback
```

#### Change 3: Commented Out Unsolicited Startup Messages
**Lines 950-953:**
```c
/* Send startup message to RTT only - don't send unsolicited UART messages */
test_run_info((unsigned char *)"OK STARTUP V2");
// Commented out: send_response("OK STARTUP V2");  // Don't send unsolicited messages
```

**Lines 1000-1002:**
```c
test_run_info((unsigned char *)"OK DW3000_READY");
// Commented out: send_response("OK DW3000_READY");  // Don't send unsolicited messages
```

**Lines 1016-1017:**
```c
test_run_info((unsigned char *)"OK MAIN_LOOP");
// Commented out: send_response("OK MAIN_LOOP");  // Don't send unsolicited messages
```

### File: `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c`

#### Change 1: Commented Out Unsolicited Startup Messages
**Lines 898-900:**
```c
/* Send startup message to RTT only - don't send unsolicited UART messages */
test_run_info((unsigned char *)"OK STARTUP V2");
// Commented out: send_response("OK STARTUP V2");  // Don't send unsolicited messages
```

**Lines 970-972:**
```c
test_run_info((unsigned char *)"OK DW3000_READY");
// Commented out: send_response("OK DW3000_READY");  // Don't send unsolicited messages
```

**Lines 984-985:**
```c
test_run_info((unsigned char *)"OK MAIN_LOOP");
// Commented out: send_response("OK MAIN_LOOP");  // Don't send unsolicited messages
```

## How to Build and Flash

### Prerequisites
- Raspberry Pi 5 with the firmware repository cloned
- Both DWM3001C boards connected via USB (J-Link programmers)
- RS-485 transceivers connected to both boards

### Step 1: Build and Flash Node A (TX)

```bash
cd ~/projects/DWM3001C-starter-firmware

# Edit example_selection.h to enable TX firmware
nano Src/example_selection.h
# Comment out: //#define TEST_ORCHESTRATOR_RX_V2
# Uncomment: #define TEST_ORCHESTRATOR_TX_V2

# Build the firmware
make clean
make

# Flash Node A (TX) - connect to first J-Link programmer
# Find the J-Link serial number: JLinkExe -ShowEmuList
# Then flash:
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN <SERIAL_NUMBER_NODE_A>
# In JLinkExe prompt:
# loadfile build/dw3000_api.hex
# r
# g
# q
```

### Step 2: Build and Flash Node B (RX)

```bash
# Edit example_selection.h to enable RX firmware
nano Src/example_selection.h
# Comment out: //#define TEST_ORCHESTRATOR_TX_V2
# Uncomment: #define TEST_ORCHESTRATOR_RX_V2

# Build the firmware
make clean
make

# Flash Node B (RX) - connect to second J-Link programmer
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN <SERIAL_NUMBER_NODE_B>
# In JLinkExe prompt:
# loadfile build/dw3000_api.hex
# r
# g
# q
```

### Step 3: Test with Orchestrator

```bash
cd ~/projects/DWM3001C-starter-firmware/Orchestrator
npm run web
```

## Expected Behavior After Fix

1. **No unsolicited messages**: Firmware will NOT send any UART messages during startup
2. **Clean command-response**: 
   - Orchestrator sends: `PNG\r\n`
   - Firmware responds: `OK\r\n`
3. **No warnings**: RS-485 controller will not show "no pending command" warnings
4. **No timeouts**: PING commands will succeed
5. **Stable operation**: No repeated reboots or crashes

## Verification Steps

1. **Check PING works**:
   ```bash
   # In orchestrator terminal, you should see:
   [RS485 TX] /dev/ttyUSB0: PNG (5 bytes including \r\n)
   [RS485 RX] /dev/ttyUSB0: OK
   ✓ Node A PING successful
   ```

2. **Check NODE_TYPE works**:
   ```bash
   # Should see:
   [RS485 TX] /dev/ttyUSB0: NODE_TYPE (11 bytes including \r\n)
   [RS485 RX] /dev/ttyUSB0: OK NODE_TYPE=TX_V2
   ✓ Node A is TX_V2
   ```

3. **Check configuration works**:
   ```bash
   # Should see:
   [RS485 TX] /dev/ttyUSB0: CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100
   [RS485 RX] /dev/ttyUSB0: OK CONFIG
   ✓ Node A configured successfully
   ```

## Debug Information

If issues persist after flashing:

1. **Check RTT output** (startup messages still visible here):
   ```bash
   JLinkRTTClient
   ```
   You should see:
   ```
   === FIRMWARE STARTING ===
   ORCHESTRATOR TX v2.0
   OK STARTUP V2
   OK DW3000_READY
   OK MAIN_LOOP
   ```

2. **Check LED behavior**:
   - **Red LED**: Firmware started (blinks twice on startup)
   - **Orange LED**: UART initialized (blinks twice on startup)
   - **Blue LED**: UART ready (blinks twice on startup)
   - **Orange LED**: Command received (blinks when command arrives)
   - **Green LED**: Response sent (blinks when response is sent)

3. **Check serial ports**:
   ```bash
   ls -l /dev/ttyUSB*
   # Should show:
   # /dev/ttyUSB0 -> Node A (TX)
   # /dev/ttyUSB1 -> Node B (RX)
   ```

4. **Test with direct serial terminal**:
   ```bash
   # Install minicom if not already installed
   sudo apt install minicom
   
   # Connect to Node A
   minicom -D /dev/ttyUSB0 -b 115200
   
   # Type: PNG<Enter>
   # Should see: OK
   ```

## Notes

- The `test_run_info()` calls are still present but only output to SEGGER RTT (debug console), not UART
- This allows debugging via RTT without interfering with RS-485 communication
- The firmware still blinks LEDs to provide visual feedback of operation
- The UART interrupt handler is now much faster and more reliable

## Related Files

- `Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c` - TX firmware
- `Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c` - RX firmware
- `Src/example_selection.h` - Example selection (choose TX or RX)
- `Src/main.c` - Main entry point (calls orchestrator_tx_v2() or orchestrator_rx_v2())
- `Orchestrator/src/serial/rs485Controller.js` - RS-485 controller (orchestrator side)
- `Orchestrator/src/orchestrator/nodeController.js` - Node controller (orchestrator side)
