# Quick Build and Flash Guide (Raspberry Pi 5)

## Prerequisites

1. **Hardware Setup**:
   - Raspberry Pi 5 with firmware repository cloned
   - 2x DWM3001C boards with J-Link programmers connected via USB
   - RS-485 transceivers connected to both boards
   - USB cables for both J-Link programmers

2. **Software Installed**:
   - J-Link software suite (JLinkExe, JLinkRTTClient)
   - ARM GCC toolchain
   - Make

## Quick Flash Commands

### Option 1: Using Helper Scripts (Recommended)

```bash
cd ~/projects/DWM3001C-starter-firmware

# Flash Node A (TX)
./build-and-flash-tx.sh <JLINK_SERIAL_NODE_A>

# Flash Node B (RX)
./build-and-flash-rx.sh <JLINK_SERIAL_NODE_B>
```

### Option 2: Manual Build and Flash

#### Build and Flash Node A (TX)

```bash
cd ~/projects/DWM3001C-starter-firmware

# 1. Configure for TX
nano Src/example_selection.h
# Comment out: //#define TEST_ORCHESTRATOR_RX_V2
# Uncomment: #define TEST_ORCHESTRATOR_TX_V2
# Save and exit (Ctrl+X, Y, Enter)

# 2. Build
make clean && make

# 3. Find J-Link serial numbers
JLinkExe -ShowEmuList
# Note the serial number for Node A

# 4. Flash Node A
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN <SERIAL_NODE_A>
# In JLinkExe prompt:
loadfile build/dw3000_api.hex
r
g
q
```

#### Build and Flash Node B (RX)

```bash
cd ~/projects/DWM3001C-starter-firmware

# 1. Configure for RX
nano Src/example_selection.h
# Comment out: //#define TEST_ORCHESTRATOR_TX_V2
# Uncomment: #define TEST_ORCHESTRATOR_RX_V2
# Save and exit (Ctrl+X, Y, Enter)

# 2. Build
make clean && make

# 3. Flash Node B
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN <SERIAL_NODE_B>
# In JLinkExe prompt:
loadfile build/dw3000_api.hex
r
g
q
```

## Verify Flash Success

### Check LED Behavior

After flashing, both boards should:
1. **Red LED**: Blink twice (firmware started)
2. **Orange LED**: Blink twice (UART initialized)
3. **Blue LED**: Blink twice (UART ready)
4. Then all LEDs off (waiting for commands)

### Test with Orchestrator

```bash
cd ~/projects/DWM3001C-starter-firmware/Orchestrator
npm run web
```

Expected output:
```
Connecting to Node A...
> Node A connected
Pinging Node A...
✓ Node A PING successful
✓ Node A is TX_V2

Connecting to Node B...
> Node B connected
Pinging Node B...
✓ Node B PING successful
✓ Node B is RX_V2
```

## Troubleshooting

### Issue: "Cannot connect to J-Link"

**Solution:**
```bash
# Check USB devices
lsusb | grep SEGGER

# Check permissions
sudo usermod -a -G dialout $USER
# Log out and log back in

# Try again
JLinkExe -ShowEmuList
```

### Issue: "PING timeout"

**Possible causes:**
1. **Wrong firmware flashed**: Check `example_selection.h` has correct `#define`
2. **RS-485 not connected**: Check wiring (A+, B-, GND)
3. **Wrong serial port**: Check `/dev/ttyUSB0` and `/dev/ttyUSB1` exist
4. **Baud rate mismatch**: Firmware uses 115200 baud

**Solution:**
```bash
# Check serial ports
ls -l /dev/ttyUSB*

# Test with minicom
minicom -D /dev/ttyUSB0 -b 115200
# Type: PNG<Enter>
# Should see: OK
```

### Issue: "Firmware keeps rebooting"

**Possible causes:**
1. **Power supply issue**: Check USB power is stable
2. **Firmware crash**: Check RTT output for error messages

**Solution:**
```bash
# Check RTT output
JLinkRTTClient
# Should see startup messages without crashes
```

### Issue: "Build fails"

**Solution:**
```bash
# Clean and rebuild
make clean
rm -rf build/
make

# Check toolchain
arm-none-eabi-gcc --version
# Should show: gcc version 10.3.1 or newer
```

## Quick Reference

### Serial Port Mapping
- `/dev/ttyUSB0` → Node A (TX)
- `/dev/ttyUSB1` → Node B (RX)

### Baud Rate
- 115200 baud, 8N1, no flow control

### LED Indicators
- **Red**: Error or startup
- **Orange**: Command received (RX)
- **Green**: Response sent (TX)
- **Blue**: UART activity

### Commands
- `PNG` → `OK` (ping/connectivity test)
- `NODE_TYPE` → `OK NODE_TYPE=TX_V2` or `OK NODE_TYPE=RX_V2`
- `CFG <params>` → `OK CONFIG` (configure UWB)
- `STRT` → `OK START` (start test)
- `STOP` → `OK STOP` (stop test)
- `STAT` → `OK STATS <data>` (get statistics)
- `RST` → `OK` (reset statistics)

## Helper Scripts to Create

### `build-and-flash-tx.sh`

```bash
#!/bin/bash
# Usage: ./build-and-flash-tx.sh <JLINK_SERIAL>

if [ -z "$1" ]; then
    echo "Usage: $0 <JLINK_SERIAL>"
    exit 1
fi

JLINK_SERIAL=$1

# Configure for TX
sed -i 's/^#define TEST_ORCHESTRATOR_RX_V2$/\/\/#define TEST_ORCHESTRATOR_RX_V2/' Src/example_selection.h
sed -i 's/^\/\/#define TEST_ORCHESTRATOR_TX_V2$/#define TEST_ORCHESTRATOR_TX_V2/' Src/example_selection.h

# Build
make clean && make

# Flash
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $JLINK_SERIAL <<EOF
loadfile build/dw3000_api.hex
r
g
q
EOF

echo "Node A (TX) flashed successfully!"
```

### `build-and-flash-rx.sh`

```bash
#!/bin/bash
# Usage: ./build-and-flash-rx.sh <JLINK_SERIAL>

if [ -z "$1" ]; then
    echo "Usage: $0 <JLINK_SERIAL>"
    exit 1
fi

JLINK_SERIAL=$1

# Configure for RX
sed -i 's/^#define TEST_ORCHESTRATOR_TX_V2$/\/\/#define TEST_ORCHESTRATOR_TX_V2/' Src/example_selection.h
sed -i 's/^\/\/#define TEST_ORCHESTRATOR_RX_V2$/#define TEST_ORCHESTRATOR_RX_V2/' Src/example_selection.h

# Build
make clean && make

# Flash
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $JLINK_SERIAL <<EOF
loadfile build/dw3000_api.hex
r
g
q
EOF

echo "Node B (RX) flashed successfully!"
```

Make scripts executable:
```bash
chmod +x build-and-flash-tx.sh
chmod +x build-and-flash-rx.sh
```
