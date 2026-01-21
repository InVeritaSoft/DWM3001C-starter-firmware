#!/bin/bash
# Build and flash Node B (RX) firmware
# Usage: ./build-and-flash-rx.sh <JLINK_SERIAL>

set -e  # Exit on error

if [ -z "$1" ]; then
    echo "Usage: $0 <JLINK_SERIAL>"
    echo ""
    echo "To find J-Link serial numbers, run:"
    echo "  echo 'ShowEmuList' | JLinkExe"
    echo ""
    echo "Or interactively:"
    echo "  JLinkExe"
    echo "  > ShowEmuList"
    exit 1
fi

JLINK_SERIAL=$1

echo "============================================"
echo "Building and Flashing Node B (RX) Firmware"
echo "============================================"
echo ""

# Configure for RX
echo "[1/4] Configuring for RX firmware..."
# Configure example_selection.h
sed -i '' 's/^#define TEST_ORCHESTRATOR_TX_V2$/\/\/#define TEST_ORCHESTRATOR_TX_V2/' Src/example_selection.h
sed -i '' 's/^\/\/#define TEST_ORCHESTRATOR_RX_V2$/#define TEST_ORCHESTRATOR_RX_V2/' Src/example_selection.h
# Configure main.c to call orchestrator_rx_v2()
# Comment out TX_V2 if it's active
sed -i '' 's/^    extern int orchestrator_tx_v2(void); orchestrator_tx_v2();/    \/\/ extern int orchestrator_tx_v2(void); orchestrator_tx_v2();/' Src/main.c
# Uncomment RX_V2 if it's commented
sed -i '' 's/^    \/\/ extern int orchestrator_rx_v2(void); orchestrator_rx_v2();/    extern int orchestrator_rx_v2(void); orchestrator_rx_v2();/' Src/main.c
echo "✓ Configured for RX"

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
echo "[3/4] Flashing Node B (RX) with J-Link serial: $JLINK_SERIAL..."
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
echo "Node B (RX) should now:"
echo "  - Blink RED LED twice (firmware started)"
echo "  - Blink ORANGE LED twice (UART initialized)"
echo "  - Blink GREEN LED twice (UART ready)"
echo "  - Then all LEDs off (waiting for commands)"
echo "  - GREEN LED will blink on each byte received from Arduino"
echo ""
echo "✓ Node B (RX) flashed successfully!"
echo ""
echo "Next steps:"
echo "  1. Test with orchestrator: cd Orchestrator && npm run web"
echo "  2. Check web UI: http://<RPI_IP>:5000"
