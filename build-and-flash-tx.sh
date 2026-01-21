#!/bin/bash
# Build and flash Node A (TX) firmware
# Usage: ./build-and-flash-tx.sh <JLINK_SERIAL>

set -e  # Exit on error

if [ -z "$1" ]; then
    echo "Usage: $0 <JLINK_SERIAL>"
    echo ""
    echo "To find J-Link serial numbers, run:"
    echo "  JLinkExe -ShowEmuList"
    exit 1
fi

JLINK_SERIAL=$1

echo "============================================"
echo "Building and Flashing Node A (TX) Firmware"
echo "============================================"
echo ""

# Configure for TX
echo "[1/4] Configuring for TX firmware..."
sed -i '' 's/^#define TEST_ORCHESTRATOR_RX_V2$/\/\/#define TEST_ORCHESTRATOR_RX_V2/' Src/example_selection.h
sed -i '' 's/^\/\/#define TEST_ORCHESTRATOR_TX_V2$/#define TEST_ORCHESTRATOR_TX_V2/' Src/example_selection.h
echo "✓ Configured for TX"

# Build
echo ""
echo "[2/4] Building firmware..."
make clean > /dev/null 2>&1
make
echo "✓ Build complete"

# Check if hex file exists
if [ ! -f "Output/Common/Exe/dw3000_api.hex" ]; then
    echo "✗ Error: Output/Common/Exe/dw3000_api.hex not found!"
    exit 1
fi

# Flash
echo ""
echo "[3/4] Flashing Node A (TX) with J-Link serial: $JLINK_SERIAL..."
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $JLINK_SERIAL <<EOF > /dev/null 2>&1
loadfile Output/Common/Exe/dw3000_api.hex
r
g
q
EOF

if [ $? -eq 0 ]; then
    echo "✓ Flash complete"
else
    echo "✗ Flash failed!"
    exit 1
fi

# Verify
echo ""
echo "[4/4] Verification..."
echo "Node A (TX) should now:"
echo "  - Blink RED LED twice (firmware started)"
echo "  - Blink ORANGE LED twice (UART initialized)"
echo "  - Blink BLUE LED twice (UART ready)"
echo "  - Then all LEDs off (waiting for commands)"
echo ""
echo "✓ Node A (TX) flashed successfully!"
echo ""
echo "Next steps:"
echo "  1. Flash Node B (RX): ./build-and-flash-rx.sh <JLINK_SERIAL_NODE_B>"
echo "  2. Test with orchestrator: cd Orchestrator && npm run web"
