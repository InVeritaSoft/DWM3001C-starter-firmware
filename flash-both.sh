#!/bin/bash
# Unified flash script for Mac
# Usage: ./flash-both.sh <TX_JLINK_SERIAL> <RX_JLINK_SERIAL>

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <TX_JLINK_SERIAL> <RX_JLINK_SERIAL>"
    echo "To find serials, run: JLinkExe -ShowEmuList"
    exit 1
fi

TX_SN=$1
RX_SN=$2

echo "--- Flashing TX Node ($TX_SN) ---"
cat <<EOF > flash_tx.jlink
loadfile dw3000_tx.hex
r
g
q
EOF
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $TX_SN -CommandFile flash_tx.jlink
rm flash_tx.jlink

echo ""
echo "--- Flashing RX Node ($RX_SN) ---"
cat <<EOF > flash_rx.jlink
loadfile dw3000_rx.hex
r
g
q
EOF
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -SelectEmuBySN $RX_SN -CommandFile flash_rx.jlink
rm flash_rx.jlink

echo ""
echo "Done! Both nodes flashed with 57600 baud firmware."
