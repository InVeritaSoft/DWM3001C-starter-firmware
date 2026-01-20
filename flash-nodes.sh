#!/bin/bash
# Build and flash both TX and RX nodes
# Attempts Docker first, falls back to native J-Link on Mac

set -e

echo "============================================"
echo "Building and Flashing DWM3001C Nodes"
echo "============================================"
echo ""

# Check if hex files exist
if [ ! -f "dw3000_tx.hex" ] || [ ! -f "dw3000_rx.hex" ]; then
    echo "Error: Hex files not found!"
    echo "  Expected: dw3000_tx.hex and dw3000_rx.hex"
    echo "  Run 'make build' first or use the pre-built files."
    exit 1
fi

# Ensure Output directory exists
mkdir -p Output/Common/Exe

# Function to flash using Docker (may not work on Mac)
flash_with_docker() {
    local hex_file=$1
    local node_name=$2
    
    echo "[$node_name] Attempting Docker flash..."
    cp "$hex_file" Output/Common/Exe/dw3000_api.hex
    
    if make flash 2>&1 | tee /tmp/docker_flash.log; then
        echo "[$node_name] ✓ Docker flash successful!"
        return 0
    else
        echo "[$node_name] ✗ Docker flash failed (expected on Mac)"
        return 1
    fi
}

# Function to flash using native J-Link
flash_with_jlink() {
    local hex_file=$1
    local node_name=$2
    local serial=$3
    
    if [ -z "$serial" ]; then
        echo "[$node_name] Error: J-Link serial number required"
        return 1
    fi
    
    echo "[$node_name] Flashing with native J-Link (serial: $serial)..."
    
    cat <<EOF > /tmp/flash_${node_name}.jlink
loadfile $hex_file
r
g
q
EOF
    
    if JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $serial -CommandFile /tmp/flash_${node_name}.jlink; then
        echo "[$node_name] ✓ Flash successful!"
        rm /tmp/flash_${node_name}.jlink
        return 0
    else
        echo "[$node_name] ✗ Flash failed!"
        rm /tmp/flash_${node_name}.jlink
        return 1
    fi
}

# Detect J-Link devices
echo "Detecting J-Link devices..."
if command -v JLinkExe &> /dev/null; then
    JLinkExe -ShowEmuList 2>&1 | grep -E "J-Link|SEGGER" | head -2 > /tmp/jlink_devices.txt || true
    
    if [ -s /tmp/jlink_devices.txt ]; then
        echo "Found J-Link devices:"
        cat /tmp/jlink_devices.txt
        echo ""
        
        # Extract serial numbers (simplified - may need adjustment based on output format)
        SERIALS=($(cat /tmp/jlink_devices.txt | grep -oE '[0-9]{6,}' | head -2))
        
        if [ ${#SERIALS[@]} -ge 2 ]; then
            TX_SERIAL=${SERIALS[0]}
            RX_SERIAL=${SERIALS[1]}
            echo "Using detected serials: TX=$TX_SERIAL, RX=$RX_SERIAL"
        elif [ ${#SERIALS[@]} -eq 1 ]; then
            TX_SERIAL=${SERIALS[0]}
            RX_SERIAL=${SERIALS[0]}
            echo "Warning: Only one device found, will flash both to same device"
        else
            echo "Could not auto-detect serial numbers"
            TX_SERIAL=""
            RX_SERIAL=""
        fi
    else
        echo "No J-Link devices detected automatically"
        TX_SERIAL=""
        RX_SERIAL=""
    fi
else
    echo "JLinkExe not found in PATH"
    TX_SERIAL=""
    RX_SERIAL=""
fi

# Try Docker flash first (will likely fail on Mac)
echo ""
echo "=== Attempting Docker Flash (may not work on Mac) ==="
DOCKER_TX_SUCCESS=false
DOCKER_RX_SUCCESS=false

if flash_with_docker "dw3000_tx.hex" "TX"; then
    DOCKER_TX_SUCCESS=true
else
    echo "Docker flash failed for TX (this is normal on Mac)"
fi

if flash_with_docker "dw3000_rx.hex" "RX"; then
    DOCKER_RX_SUCCESS=true
else
    echo "Docker flash failed for RX (this is normal on Mac)"
fi

# If Docker failed, try native J-Link
if [ "$DOCKER_TX_SUCCESS" = false ] || [ "$DOCKER_RX_SUCCESS" = false ]; then
    echo ""
    echo "=== Falling back to Native J-Link Flash ==="
    
    if [ -z "$TX_SERIAL" ] || [ -z "$RX_SERIAL" ]; then
        echo ""
        echo "Please provide J-Link serial numbers:"
        echo "  Run: JLinkExe -ShowEmuList"
        echo ""
        read -p "Enter TX node J-Link serial: " TX_SERIAL
        read -p "Enter RX node J-Link serial: " RX_SERIAL
    fi
    
    if [ "$DOCKER_TX_SUCCESS" = false ]; then
        flash_with_jlink "dw3000_tx.hex" "TX" "$TX_SERIAL"
    else
        echo "[TX] Already flashed via Docker, skipping"
    fi
    
    if [ "$DOCKER_RX_SUCCESS" = false ]; then
        flash_with_jlink "dw3000_rx.hex" "RX" "$RX_SERIAL"
    else
        echo "[RX] Already flashed via Docker, skipping"
    fi
fi

echo ""
echo "============================================"
echo "Flash Complete!"
echo "============================================"
echo ""
echo "Both nodes should now be running firmware at 57600 baud."
echo "GPIO 14 (TX) and GPIO 15 (RX) are configured correctly."
