#!/bin/bash
# Flash both TX and RX nodes
# Usage: ./flash-both-nodes.sh [TX_SERIAL] [RX_SERIAL]

set -e

export PATH="$HOME/.local/bin:$PATH"

# Find JLinkExe
JLINKEXE=""
if command -v JLinkExe &> /dev/null; then
    JLINKEXE=$(command -v JLinkExe)
elif [ -f "$HOME/.local/bin/JLinkExe" ]; then
    JLINKEXE="$HOME/.local/bin/JLinkExe"
elif [ -f "/Applications/SEGGER/JLink/JLinkExe" ]; then
    JLINKEXE="/Applications/SEGGER/JLink/JLinkExe"
else
    echo "ERROR: JLinkExe not found!"
    exit 1
fi

echo "Using J-Link: $JLINKEXE"
echo ""

# Check hex files
if [ ! -f "dw3000_tx.hex" ] || [ ! -f "dw3000_rx.hex" ]; then
    echo "Error: Hex files not found!"
    exit 1
fi

# Function to get serial from connected device
get_serial() {
    "$JLINKEXE" <<EOF 2>&1 | grep "S/N:" | head -1 | grep -oE '[0-9]{6,}' || echo ""
q
EOF
}

# Function to flash a node
flash_node() {
    local hex_file=$1
    local node_name=$2
    local serial=$3
    
    echo "============================================"
    echo "Flashing $node_name Node"
    if [ -n "$serial" ]; then
        echo "Serial: $serial"
    fi
    echo "============================================"
    
    cat <<EOF > /tmp/flash_${node_name}.jlink
loadfile $(pwd)/$hex_file
r
g
q
EOF
    
    if [ -n "$serial" ]; then
        # Use specific serial
        if "$JLINKEXE" -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $serial -CommandFile /tmp/flash_${node_name}.jlink > /tmp/flash_${node_name}.log 2>&1; then
            echo "✓ $node_name flashed successfully!"
            rm /tmp/flash_${node_name}.jlink
            return 0
        else
            echo "✗ $node_name flash failed!"
            echo "Error log:"
            tail -10 /tmp/flash_${node_name}.log
            rm /tmp/flash_${node_name}.jlink
            return 1
        fi
    else
        # Auto-connect to first available
        if "$JLINKEXE" -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -CommandFile /tmp/flash_${node_name}.jlink > /tmp/flash_${node_name}.log 2>&1; then
            echo "✓ $node_name flashed successfully!"
            rm /tmp/flash_${node_name}.jlink
            return 0
        else
            echo "✗ $node_name flash failed!"
            echo "Error log:"
            tail -10 /tmp/flash_${node_name}.log
            rm /tmp/flash_${node_name}.jlink
            return 1
        fi
    fi
}

# Get serial numbers
if [ "$#" -eq 2 ]; then
    TX_SERIAL=$1
    RX_SERIAL=$2
    echo "Using provided serials: TX=$TX_SERIAL, RX=$RX_SERIAL"
elif [ "$#" -eq 1 ]; then
    TX_SERIAL=$1
    echo "TX serial provided: $TX_SERIAL"
    echo "Detecting RX serial..."
    RX_SERIAL=$(get_serial)
    if [ -z "$RX_SERIAL" ]; then
        echo "Could not auto-detect RX serial. Please provide both:"
        echo "  ./flash-both-nodes.sh <TX_SERIAL> <RX_SERIAL>"
        exit 1
    fi
    echo "Detected RX serial: $RX_SERIAL"
else
    # Try to detect both
    echo "Detecting connected devices..."
    TX_SERIAL=$(get_serial)
    if [ -z "$TX_SERIAL" ]; then
        echo "No devices detected. Please:"
        echo "  1. Connect TX board and run: ./flash-both-nodes.sh"
        echo "  2. Or provide serials: ./flash-both-nodes.sh <TX_SERIAL> <RX_SERIAL>"
        exit 1
    fi
    echo "Detected device serial: $TX_SERIAL"
    echo ""
    echo "For two devices, please provide both serials:"
    echo "  ./flash-both-nodes.sh <TX_SERIAL> <RX_SERIAL>"
    echo ""
    echo "Or flash one at a time by disconnecting/reconnecting."
    echo ""
    read -p "Flash TX node now with serial $TX_SERIAL? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        flash_node "dw3000_tx.hex" "TX" "$TX_SERIAL"
        echo ""
        echo "Now disconnect TX and connect RX, then press Enter..."
        read -p "Press Enter when RX is connected..."
        RX_SERIAL=$(get_serial)
        if [ -n "$RX_SERIAL" ]; then
            flash_node "dw3000_rx.hex" "RX" "$RX_SERIAL"
        else
            echo "Could not detect RX device"
            exit 1
        fi
    else
        exit 0
    fi
    exit 0
fi

# Flash both nodes
flash_node "dw3000_tx.hex" "TX" "$TX_SERIAL"
echo ""
flash_node "dw3000_rx.hex" "RX" "$RX_SERIAL"

echo ""
echo "============================================"
echo "✓ Both Nodes Flashed Successfully!"
echo "============================================"
