#!/bin/bash
# Build and flash both TX and RX nodes on Mac
# Since Docker cannot access USB on Mac, this uses native J-Link tools

set -e

echo "============================================"
echo "Flashing DWM3001C Nodes (Mac)"
echo "============================================"
echo ""

# Check if hex files exist
if [ ! -f "dw3000_tx.hex" ] || [ ! -f "dw3000_rx.hex" ]; then
    echo "Error: Hex files not found!"
    echo "  Expected: dw3000_tx.hex and dw3000_rx.hex"
    exit 1
fi

# Find JLinkExe
JLINKEXE=""
if command -v JLinkExe &> /dev/null; then
    JLINKEXE=$(command -v JLinkExe)
elif [ -f "/Applications/SEGGER/JLink/JLinkExe" ]; then
    JLINKEXE="/Applications/SEGGER/JLink/JLinkExe"
elif [ -f "/usr/local/bin/JLinkExe" ]; then
    JLINKEXE="/usr/local/bin/JLinkExe"
else
    echo "ERROR: JLinkExe not found!"
    echo ""
    echo "Please install SEGGER J-Link Software:"
    echo "  1. Download from: https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack"
    echo "  2. Install the macOS version"
    echo "  3. Add to PATH or run this script again"
    echo ""
    exit 1
fi

echo "Found J-Link: $JLINKEXE"
echo ""

# Detect J-Link devices
echo "Detecting J-Link devices..."
"$JLINKEXE" -ShowEmuList 2>&1 | grep -E "J-Link|SEGGER" > /tmp/jlink_list.txt || true

if [ ! -s /tmp/jlink_list.txt ]; then
    echo "ERROR: No J-Link devices detected!"
    echo ""
    echo "Please:"
    echo "  1. Connect both DWM3001CDK boards via USB (J9 port)"
    echo "  2. Power on both boards"
    echo "  3. Run: $JLINKEXE -ShowEmuList"
    echo "  4. Note the serial numbers and run this script with:"
    echo "     ./flash-nodes-mac.sh <TX_SERIAL> <RX_SERIAL>"
    echo ""
    exit 1
fi

echo "Found devices:"
cat /tmp/jlink_list.txt
echo ""

# Extract serial numbers (try multiple patterns)
SERIALS=($("$JLINKEXE" -ShowEmuList 2>&1 | grep -oE '[0-9]{6,}' | sort -u | head -2))

if [ "$#" -eq 2 ]; then
    # User provided serials as arguments
    TX_SERIAL=$1
    RX_SERIAL=$2
    echo "Using provided serials: TX=$TX_SERIAL, RX=$RX_SERIAL"
elif [ ${#SERIALS[@]} -ge 2 ]; then
    # Auto-detect: use first two unique serials
    TX_SERIAL=${SERIALS[0]}
    RX_SERIAL=${SERIALS[1]}
    echo "Auto-detected serials: TX=$TX_SERIAL, RX=$RX_SERIAL"
elif [ ${#SERIALS[@]} -eq 1 ]; then
    # Only one device - ask user
    TX_SERIAL=${SERIALS[0]}
    echo "Only one device detected (serial: $TX_SERIAL)"
    echo "Will flash TX first, then prompt to swap/reconnect for RX"
    read -p "Press Enter to continue with TX flash..."
    RX_SERIAL=""
else
    echo "Could not determine serial numbers"
    echo "Please run: $JLINKEXE -ShowEmuList"
    echo "Then run this script with: ./flash-nodes-mac.sh <TX_SERIAL> <RX_SERIAL>"
    exit 1
fi

# Flash TX node
echo ""
echo "============================================"
echo "Flashing TX Node (Serial: $TX_SERIAL)"
echo "============================================"

cat <<EOF > /tmp/flash_tx.jlink
loadfile $(pwd)/dw3000_tx.hex
r
g
q
EOF

if "$JLINKEXE" -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $TX_SERIAL -CommandFile /tmp/flash_tx.jlink; then
    echo "✓ TX Node flashed successfully!"
    rm /tmp/flash_tx.jlink
else
    echo "✗ TX Node flash failed!"
    rm /tmp/flash_tx.jlink
    exit 1
fi

# Flash RX node
if [ -n "$RX_SERIAL" ] && [ "$RX_SERIAL" != "$TX_SERIAL" ]; then
    echo ""
    echo "============================================"
    echo "Flashing RX Node (Serial: $RX_SERIAL)"
    echo "============================================"
    
    cat <<EOF > /tmp/flash_rx.jlink
loadfile $(pwd)/dw3000_rx.hex
r
g
q
EOF
    
    if "$JLINKEXE" -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $RX_SERIAL -CommandFile /tmp/flash_rx.jlink; then
        echo "✓ RX Node flashed successfully!"
        rm /tmp/flash_rx.jlink
    else
        echo "✗ RX Node flash failed!"
        rm /tmp/flash_rx.jlink
        exit 1
    fi
else
    echo ""
    echo "============================================"
    echo "Flashing RX Node"
    echo "============================================"
    echo "Please disconnect TX board and connect RX board, then press Enter..."
    read -p "Press Enter when RX board is connected..."
    
    # Re-detect
    "$JLINKEXE" -ShowEmuList 2>&1 | grep -oE '[0-9]{6,}' | sort -u > /tmp/rx_serials.txt
    RX_SERIAL=$(head -1 /tmp/rx_serials.txt)
    
    if [ -z "$RX_SERIAL" ]; then
        echo "Error: Could not detect RX board serial number"
        exit 1
    fi
    
    echo "Detected RX serial: $RX_SERIAL"
    
    cat <<EOF > /tmp/flash_rx.jlink
loadfile $(pwd)/dw3000_rx.hex
r
g
q
EOF
    
    if "$JLINKEXE" -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $RX_SERIAL -CommandFile /tmp/flash_rx.jlink; then
        echo "✓ RX Node flashed successfully!"
        rm /tmp/flash_rx.jlink
    else
        echo "✗ RX Node flash failed!"
        rm /tmp/flash_rx.jlink
        exit 1
    fi
fi

echo ""
echo "============================================"
echo "✓ Both Nodes Flashed Successfully!"
echo "============================================"
echo ""
echo "Firmware Details:"
echo "  - Baud Rate: 57600 (optimized for Arduino bridge)"
echo "  - TX Pin: GPIO 14 (J10 Pin 8)"
echo "  - RX Pin: GPIO 15 (J10 Pin 10)"
echo ""
echo "Next steps:"
echo "  1. Connect Arduino bridges to both nodes"
echo "  2. Start Node.js orchestrator: cd Orchestrator && npm run web"
echo "  3. Test communication via web interface"
echo ""
