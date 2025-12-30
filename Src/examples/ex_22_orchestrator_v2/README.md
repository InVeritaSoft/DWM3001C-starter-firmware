# Orchestrator Example v2 - Enhanced Build and Flash Guide

## Overview

This is an enhanced version of the orchestrator example with significant improvements over v1:

### Key Improvements

#### TX Variant (orchestrator_tx_v2.c)
- **Enhanced TX Power Control**: Uses `dwt_adjust_tx_power()` API for regulatory-compliant power boost calculation
- **Frame Duration Calculation**: Automatically calculates frame duration for power boost compliance
- **Comprehensive Error Tracking**: Tracks TX errors, timeouts, and transmission attempts separately
- **Enhanced Statistics**: Provides detailed statistics including frame duration, TX timestamps, and error categorization
- **Better State Management**: Improved error handling and recovery

#### RX Variant (orchestrator_rx_v2.c)
- **Enhanced RF Diagnostics**: Uses `dwt_readdiagnostics()` for comprehensive RF metrics
- **Proper RSSI Calculation**: Improved RSSI calculation from channel power measurements
- **First Path Analysis**: Tracks first path index and power for channel analysis
- **Error Categorization**: Separates CRC errors, PHY errors, timeouts, and buffer overruns
- **Event Counter Tracking**: Uses `dwt_readeventcounters()` for hardware-level statistics
- **Min/Max Tracking**: Tracks minimum and maximum RSSI and preamble quality values
- **Better Statistics Aggregation**: More accurate averaging and comprehensive statistics reporting

## Pin Configuration

### UART Pins (RS-485 Communication)
- **RX Pin**: GPIO 15 (P0.15) - Receives commands from RS-485 (Pin 10 on J10)
- **TX Pin**: GPIO 14 (P0.14) - Sends responses via RS-485 (Pin 8 on J10)
- **RS-485 DE/RE Pin**: GPIO P0.06 (RS485_DE_RE_PIN) - **CRITICAL for direction control**
  - **LOW (0)**: RX mode - Receives data from RS-485 bus
  - **HIGH (1)**: TX mode - Drives data onto RS-485 bus
- **Baud Rate**: 115200

**Note**: Pins 8 and 10 are used for:
- **Pin 8**: DW3000_IRQ_Pin (interrupt pin for UWB chip)
- **Pin 10**: DW3000_CS_Pin (SPI chip select for UWB chip)

The UART for RS-485 communication uses **GPIO 15 (RX) and GPIO 14 (TX)**, which correspond to **Pin 10 and Pin 8 on J10 connector**.

**IMPORTANT**: The RS-485 transceiver's DE/RE pin **MUST** be connected to **GPIO P0.06** for proper bidirectional communication!

## Building and Flashing

### Step 1: Build TX Variant (Node A)

1. Edit `Src/example_selection.h`:
   ```c
   // Comment out other examples:
   //#define TEST_ORCHESTRATOR_TX
   
   // Uncomment TX v2 variant:
   #define TEST_ORCHESTRATOR_TX_V2
   ```

2. Edit `Src/main.c`:
   ```c
   // Comment out other examples:
   // extern int orchestrator_tx(void); orchestrator_tx();
   
   // Uncomment TX v2 variant:
   extern int orchestrator_tx_v2(void); orchestrator_tx_v2();
   ```

3. Build:
   ```bash
   make build
   ```

4. Flash to Node A board:
   ```bash
   make flash
   ```

### Step 2: Build RX Variant (Node B)

1. Edit `Src/example_selection.h`:
   ```c
   // Comment out TX v2 variant:
   //#define TEST_ORCHESTRATOR_TX_V2
   
   // Uncomment RX v2 variant:
   #define TEST_ORCHESTRATOR_RX_V2
   ```

2. Edit `Src/main.c`:
   ```c
   // Comment out TX v2 variant:
   // extern int orchestrator_tx_v2(void); orchestrator_tx_v2();
   
   // Uncomment RX v2 variant:
   extern int orchestrator_rx_v2(void); orchestrator_rx_v2();
   ```

3. Build:
   ```bash
   make build
   ```

4. Flash to Node B board:
   ```bash
   make flash
   ```

**Important**: You need to build and flash **separately** for each board. The firmware is compiled with either TX or RX code, not both.

## Enhanced Command Protocol

### TX v2 Commands

#### Configuration (Enhanced)
```
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=30 rate_hz=100
```

**New Parameters:**
- `pwr_ref=<hex>`: Reference TX power setting (default: 0x36363636)
- `boost=<value>`: Power boost in 0.1dB steps (0-113, automatically clamped to regulatory limits)

**Note**: The boost value is automatically limited based on frame duration to ensure regulatory compliance.

#### Statistics (Enhanced)
```
STAT
```

**Response Format:**
```
OK STATS sent=<total> attempted=<total> errors=<count> timeouts=<count> last_err=<code> frame_dur=<us>
```

**New Statistics:**
- `attempted`: Total transmission attempts
- `errors`: General TX errors
- `timeouts`: TX timeout errors
- `frame_dur`: Calculated frame duration in microseconds

### RX v2 Commands

#### Statistics (Enhanced)
```
STAT
```

**Response Format:**
```
OK STATS rx=<total> lost=<count> crc_err=<count> phy_err=<count> timeout=<count> overrun=<count>
      rssi_avg=<dBm> rssi_min=<dBm> rssi_max=<dBm> 
      pre_q_avg=<value> pre_q_min=<value> pre_q_max=<value>
      fp_index_avg=<value> evt_crc_good=<count> evt_crc_bad=<count>
```

**New Statistics:**
- `phy_err`: PHY header errors
- `timeout`: RX timeout errors
- `overrun`: RX buffer overrun errors
- `rssi_min/max`: Minimum and maximum RSSI values
- `pre_q_min/max`: Minimum and maximum preamble quality values
- `fp_index_avg`: Average first path index
- `evt_crc_good/bad`: Hardware event counter values

## Technical Details

### TX Power Boost Calculation

The TX v2 variant uses the `dwt_adjust_tx_power()` API which:
1. Calculates frame duration based on preamble length, data rate, and payload size
2. Determines maximum allowed boost using `calculate_power_boost()` function
3. Applies the smaller of configured boost or calculated maximum boost
4. Configures TX power with regulatory compliance

**Example:**
- Frame duration: 200μs
- Maximum boost: ~30 (3.0 dB)
- Configured boost: 50 (5.0 dB)
- Applied boost: 30 (clamped to maximum)

### RX Diagnostics

The RX v2 variant uses comprehensive diagnostics:

1. **Channel Power**: Extracted from `ipatovPower` field
2. **RSSI Calculation**: Simplified calculation from channel power (requires calibration for production)
3. **First Path Detection**: Uses `ipatovFpIndex` for first path analysis
4. **Preamble Quality**: Uses `ipatovAccumCount` as quality indicator
5. **Event Counters**: Hardware-level CRC good/bad counters

### Error Handling

#### TX Errors
- **Error Code 0**: Success
- **Error Code 1**: General TX error
- **Error Code 2**: TX timeout

#### RX Errors
- **CRC Errors**: Frame received but CRC check failed
- **PHY Errors**: PHY header decoding failed
- **Timeout Errors**: No frame received within timeout period
- **Overrun Errors**: RX buffer overflow

## LED Behavior

The firmware uses `dwt_setleds()` which controls the **DW3000 UWB chip's internal LEDs**, not the board LEDs.

- **On startup**: LEDs will blink once during initialization
- **During operation**: LEDs may flash during UWB activity (TX/RX events)
- **Board LEDs**: The DWM3001CDK board LEDs (LED_1, LED_2, LED_3, LED_4) are controlled as follows:
  - **LED 0 (Red)**: Error states and initialization
  - **LED 1 (Orange)**: Command received via UART
  - **LED 2 (Green)**: Response sent via UART
  - **LED 3 (Blue)**: UART initialized and standby state

## RS-485 Connection

**CRITICAL**: Connect the RS-485 transceiver DE/RE pin to **GPIO P0.06** for proper direction control!

Connect the RS-485 transceiver to:
- **A+**: Connect to RS-485 A+ line (differential positive)
- **B-**: Connect to RS-485 B- line (differential negative)
- **GND**: Connect to ground
- **DI (Data In)**: Connect to GPIO 14 (P0.14) - UART TX (Pin 8 on J10)
- **RO (Receive Out)**: Connect to GPIO 15 (P0.15) - UART RX (Pin 10 on J10)
- **DE/RE (Direction Enable)**: Connect to **GPIO P0.06** (RS485_DE_RE_PIN)
  - If your transceiver has separate DE and RE pins, connect both to P0.06

**Termination**: Use 120Ω termination resistors at each end of the RS-485 bus (between A+ and B-).

The firmware communicates at **115200 baud** with the Orchestrator.

## Testing

After flashing:

1. **Connect RS-485** from each board to the Orchestrator (RPi 5)
2. **Power on** both boards
3. **Run the Orchestrator**:
   ```bash
   cd Orchestrator
   npm start
   ```

The Orchestrator will:
- Send `PNG` (PING) commands to verify connectivity
- Configure both nodes with `CFG` commands (now supports power boost)
- Start tests with `STRT` commands
- Collect enhanced statistics with `STAT` commands

## Comparison: v1 vs v2

### TX Variant

| Feature | v1 | v2 |
|---------|----|----|
| TX Power Control | Basic (fixed) | Enhanced (boost calculation) |
| Frame Duration | Not calculated | Automatically calculated |
| Error Tracking | Basic (last_error) | Comprehensive (errors, timeouts, attempts) |
| Statistics | Minimal | Detailed with frame duration |
| Regulatory Compliance | Manual | Automatic (boost clamping) |

### RX Variant

| Feature | v1 | v2 |
|---------|----|----|
| RF Diagnostics | Simplified | Comprehensive (full diagnostics) |
| RSSI Calculation | Basic approximation | Improved calculation |
| Error Categorization | CRC only | CRC, PHY, timeout, overrun |
| Event Counters | Not used | Hardware-level tracking |
| Statistics | Basic averages | Min/max tracking + averages |
| First Path Analysis | Not available | First path index tracking |

## Troubleshooting

### UART Not Working
- Verify pins 15 (RX) and 19 (TX) are connected correctly
- Check RS-485 transceiver wiring (A+, B-, GND)
- Verify baud rate is 115200
- Check that RS-485 termination resistors are present (120Ω at each end)

### Wrong Variant Flashed
- Make sure you build and flash separately for each board
- Check `example_selection.h` has the correct `#define` uncommented
- Check `main.c` has the correct function call uncommented

### TX Power Issues
- Verify reference power setting (`pwr_ref`) is appropriate for your hardware
- Check that boost value doesn't exceed regulatory limits (automatically clamped)
- Frame duration calculation may affect maximum allowed boost

### RX Statistics Issues
- Ensure diagnostics are enabled (automatically enabled in v2)
- RSSI values may require calibration for accurate measurements
- Event counters reset when statistics are reset

### LEDs Not Flashing
- The firmware controls DW3000 internal LEDs, not board LEDs
- Board LEDs are controlled separately (see LED Behavior section)
- Check UWB activity with the Orchestrator's statistics

## Migration from v1

To migrate from v1 to v2:

1. **Update example_selection.h**: Change `TEST_ORCHESTRATOR_TX` to `TEST_ORCHESTRATOR_TX_V2` (and RX equivalent)
2. **Update main.c**: Change function calls from `orchestrator_tx()` to `orchestrator_tx_v2()` (and RX equivalent)
3. **Update Orchestrator**: The command protocol is backward compatible, but enhanced statistics provide more information
4. **Review Configuration**: Consider using `pwr_ref` and `boost` parameters for TX power control

## Notes

- **RSSI Calibration**: The RSSI calculation in v2 is simplified and may require calibration for accurate measurements in production systems
- **Power Boost**: The power boost feature automatically ensures regulatory compliance by clamping boost values based on frame duration
- **Event Counters**: Hardware event counters provide accurate CRC statistics but reset when statistics are reset
- **Frame Duration**: Frame duration calculation is approximate and may vary slightly from actual transmission time

## References

- Qorvo DWM3001C SDK Documentation
- DW3000 API Guide
- IEEE 802.15.4 UWB Standard
- Regulatory compliance guidelines for UWB systems

