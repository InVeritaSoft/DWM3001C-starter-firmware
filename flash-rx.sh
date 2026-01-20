#!/bin/bash
# Flash RX node
# Usage: ./flash-rx.sh [SERIAL_NUMBER]

export PATH="$HOME/.local/bin:$PATH"

JLINKEXE="$HOME/.local/bin/JLinkExe"

if [ ! -f "dw3000_rx.hex" ]; then
    echo "Error: dw3000_rx.hex not found!"
    exit 1
fi

SERIAL=${1:-""}

if [ -z "$SERIAL" ]; then
    echo "Detecting J-Link device..."
    SERIAL=$("$JLINKEXE" <<'EOF' 2>&1 | grep "S/N:" | head -1 | grep -oE '[0-9]{6,}' || echo ""
q
EOF
)
    if [ -z "$SERIAL" ]; then
        echo "Error: Could not detect device serial"
        echo "Please provide serial: ./flash-rx.sh <SERIAL>"
        exit 1
    fi
    echo "Detected serial: $SERIAL"
fi

echo "Flashing RX node (serial: $SERIAL)..."

cat <<EOF > /tmp/flash_rx.jlink
loadfile $(pwd)/dw3000_rx.hex
r
g
q
EOF

if "$JLINKEXE" -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $SERIAL -CommandFile /tmp/flash_rx.jlink; then
    echo "✓ RX Node flashed successfully!"
    rm /tmp/flash_rx.jlink
else
    echo "✗ RX Node flash failed!"
    rm /tmp/flash_rx.jlink
    exit 1
fi
