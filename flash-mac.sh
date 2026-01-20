#!/bin/bash
# Helper script to flash DWM3001CDK from Mac
# Requires SEGGER J-Link software installed on the Mac

if [ ! -f "Output/Common/Exe/dw3000_api.hex" ]; then
    echo "Error: hex file not found in Output/Common/Exe/dw3000_api.hex"
    echo "Run 'make build' first."
    exit 1
fi

# Create a temporary J-Link script
cat <<EOF > flash.jlink
loadfile Output/Common/Exe/dw3000_api.hex
r
g
q
EOF

# Run JLinkExe
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -CommandFile flash.jlink

# Cleanup
rm flash.jlink
