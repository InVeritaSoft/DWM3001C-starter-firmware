# Simple UWB Examples - Quick Start Guide

## Overview

These are the simplest UWB examples that demonstrate basic TX (transmit) and RX (receive) functionality. They are perfect for:
- Learning how UWB communication works
- Testing basic hardware functionality
- Understanding the foundation before moving to more complex examples

## Examples

### 1. Simple TX (`ex_01a_simple_tx`)
**What it does:**
- Transmits UWB frames every 500ms
- Sends IEEE 802.15.4e blink frames
- Increments sequence number with each transmission
- Uses default UWB configuration

**Configuration:**
- Channel: 5
- Data Rate: 6.8 Mbps
- Preamble Length: 128 symbols
- Preamble Code: 9
- Frame Type: Blink (0xC5)

**Output:**
- Debug messages via RTT: "TX Frame Sent" for each transmission
- DW3000 internal LED flashes on each TX

### 2. Simple RX (`ex_02a_simple_rx`)
**What it does:**
- Continuously listens for UWB frames
- Receives and processes incoming frames
- Handles CRC errors automatically
- Uses same configuration as TX (must match for communication)

**Configuration:**
- Channel: 5 (must match TX)
- Data Rate: 6.8 Mbps (must match TX)
- Preamble Length: 128 symbols (must match TX)
- Preamble Code: 9 (must match TX)

**Output:**
- Debug messages via RTT: "Frame Received" for each successful reception
- DW3000 internal LED flashes on each RX

## Quick Start

### Option 1: Using PowerShell Scripts (Recommended)

#### For Simple TX:
```powershell
.\set-simple-tx.ps1
make build
make flash
```

#### For Simple RX:
```powershell
.\set-simple-rx.ps1
make build
make flash
```

### Option 2: Manual Configuration

#### Step 1: Configure `Src/example_selection.h`

**For Simple TX:**
```c
#define TEST_SIMPLE_TX
// Comment out other examples
```

**For Simple RX:**
```c
#define TEST_SIMPLE_RX
// Comment out other examples
```

#### Step 2: Configure `Src/main.c`

**For Simple TX:**
```c
extern int simple_tx(void); simple_tx();
// Comment out other examples
```

**For Simple RX:**
```c
extern int simple_rx(void); simple_rx();
// Comment out other examples
```

#### Step 3: Build and Flash
```bash
make build
make flash
```

## Testing

### Basic Test Setup

1. **Flash Simple TX to Board A**
   ```powershell
   .\set-simple-tx.ps1
   make build
   make flash
   ```

2. **Flash Simple RX to Board B**
   ```powershell
   .\set-simple-rx.ps1
   make build
   make flash
   ```

3. **Monitor Output**
   - Connect both boards via USB
   - Use RTT Viewer or serial terminal to see debug output
   - Board A (TX) will show: "TX Frame Sent" every 500ms
   - Board B (RX) will show: "Frame Received" when frames are received

### Expected Behavior

**Simple TX (Board A):**
- Starts transmitting immediately after boot
- Sends frames every 500ms
- Sequence number increments: 0, 1, 2, 3...
- LED flashes on each transmission

**Simple RX (Board B):**
- Starts listening immediately after boot
- Receives frames from Board A
- Displays "Frame Received" message
- LED flashes on each reception
- Automatically re-enables RX after each frame

### Troubleshooting

**No frames received:**
- Verify both boards use the same channel (5)
- Verify both boards use the same data rate (6.8 Mbps)
- Verify both boards use the same preamble length (128)
- Verify both boards use the same preamble code (9)
- Check physical distance between boards (should be within UWB range)
- Verify antennas are properly connected

**TX not working:**
- Check RTT output for initialization messages
- Verify DW3000 initialization succeeded
- Check LED behavior (should flash on TX)

**RX not working:**
- Check RTT output for initialization messages
- Verify RX is enabled (should be automatic)
- Check for error messages in RTT output

## Code Structure

### Simple TX Flow
```
1. Initialize SPI and GPIO
2. Reset DW3000
3. Probe for DW3000 device
4. Initialize DW3000
5. Configure UWB parameters
6. Configure TX power
7. Loop forever:
   a. Prepare frame data
   b. Write frame to DW3000
   c. Start transmission
   d. Wait for TX complete
   e. Clear status
   f. Delay 500ms
   g. Increment sequence number
```

### Simple RX Flow
```
1. Initialize SPI and GPIO
2. Reset DW3000
3. Probe for DW3000 device
4. Initialize DW3000
5. Configure UWB parameters
6. Loop forever:
   a. Clear RX buffer
   b. Enable RX
   c. Wait for frame or error
   d. If good frame:
      - Read frame data
      - Clear status
      - Display message
   e. If error:
      - Clear error status
      - Continue (RX re-enabled automatically)
```

## Key Differences from Orchestrator Examples

| Feature | Simple Examples | Orchestrator Examples |
|---------|----------------|---------------------|
| UWB TX/RX | ✅ | ✅ |
| RS-485 Communication | ❌ | ✅ |
| Command Parsing | ❌ | ✅ |
| Statistics Tracking | ❌ | ✅ |
| Configurable Parameters | ❌ | ✅ |
| Timer-based TX | ❌ | ✅ |
| Error Statistics | ❌ | ✅ |
| Power Control | Basic | Advanced |

## Next Steps

After mastering the simple examples, you can move to:

1. **Orchestrator Examples** (`ex_21_orchestrator` or `ex_22_orchestrator_v2`)
   - Adds RS-485 communication
   - Adds command parsing
   - Adds statistics tracking
   - Adds configurable parameters

2. **Two-Way Ranging (TWR) Examples**
   - `ex_05a_ds_twr_init` / `ex_05b_ds_twr_resp` - Double-sided TWR
   - `ex_06a_ss_twr_initiator` / `ex_06b_ss_twr_responder` - Single-sided TWR

3. **Advanced Features**
   - `ex_01i_simple_tx_aes` / `ex_02i_simple_rx_aes` - AES encryption
   - `ex_01h_simple_tx_pdoa` / `ex_02h_simple_rx_pdoa` - Phase Difference of Arrival

## Files

- **Simple TX**: `Src/examples/ex_01a_simple_tx/simple_tx.c`
- **Simple RX**: `Src/examples/ex_02a_simple_rx/simple_rx.c`
- **Helper Scripts**: 
  - `set-simple-tx.ps1` - Configure for TX
  - `set-simple-rx.ps1` - Configure for RX

## Notes

- These examples use **polling mode** (not interrupts) for simplicity
- Frame format follows IEEE 802.15.4e blink frame standard
- Default TX power is used (no custom power adjustment)
- No error recovery beyond basic status clearing
- No statistics tracking (just basic success/failure)

## References

- DW3000 API Documentation
- IEEE 802.15.4 UWB Standard
- DWM3001C SDK User Guide

