#!/bin/bash
# Simple build and flash script with J-Link device selection
# Usage: ./build-and-flash-simple.sh [JLINK_SERIAL]

set -e  # Exit on error

echo "============================================"
echo "Building and Flashing Firmware"
echo "============================================"
echo ""

# Build
echo "[1/3] Building firmware..."
make clean > /dev/null 2>&1
make
echo "✓ Build complete"

# Check if hex file exists
if [ ! -f "Output/Common/Exe/dw3000_api.hex" ]; then
    echo "✗ Error: Output/Common/Exe/dw3000_api.hex not found!"
    exit 1
fi

# Select J-Link device
echo ""
echo "[2/3] Selecting J-Link device..."

if [ -n "$1" ]; then
    # Use provided serial number
    JLINK_SERIAL=$1
    echo "Using provided J-Link serial: $JLINK_SERIAL"
else
    # List available J-Link devices
    echo "Scanning for J-Link devices..."
    echo ""
    
    # Get list of J-Link devices
    TEMP_FILE=$(mktemp)
    echo "ShowEmuList" | JLinkExe > "$TEMP_FILE" 2>&1 || true
    
    # Display available devices
    if grep -qE "^[0-9]+:" "$TEMP_FILE"; then
        echo "Available J-Link devices:"
        grep -E "^[0-9]+:" "$TEMP_FILE" | sed 's/^/  /'
        echo ""
    elif grep -qi "j-link" "$TEMP_FILE"; then
        echo "Found J-Link devices:"
        grep -i "j-link" "$TEMP_FILE" | head -5 | sed 's/^/  /'
        echo ""
    else
        echo "Note: Could not automatically list devices."
        echo "You can enter a serial number manually or press Enter to auto-detect."
        echo ""
    fi
    
    rm -f "$TEMP_FILE"
    
    read -p "Enter J-Link serial number (or press Enter to auto-detect): " JLINK_SERIAL
fi

# Flash
echo ""
echo "[3/3] Flashing firmware..."

if [ -n "$JLINK_SERIAL" ]; then
    echo "Using J-Link serial: $JLINK_SERIAL"
    JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $JLINK_SERIAL <<EOF > /dev/null 2>&1
loadfile Output/Common/Exe/dw3000_api.hex
r
g
q
EOF
else
    echo "Auto-detecting J-Link..."
    JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 <<EOF > /dev/null 2>&1
loadfile Output/Common/Exe/dw3000_api.hex
r
g
q
EOF
fi

if [ $? -eq 0 ]; then
    echo "✓ Flash complete"
    echo ""
    echo "✓ Firmware flashed successfully!"
else
    echo "✗ Flash failed!"
    exit 1
fi
