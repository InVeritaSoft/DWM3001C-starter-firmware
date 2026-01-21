#!/bin/bash
# Monitor Arduino Serial Output
# Usage: ./monitor-arduino.sh [port]
# Example: ./monitor-arduino.sh /dev/cu.usbmodem0007602011681

PORT="${1:-/dev/cu.usbmodem0007602011681}"
BAUD=115200

echo "=========================================="
echo "Arduino Serial Monitor"
echo "=========================================="
echo "Port: $PORT"
echo "Baud: $BAUD"
echo "Press Ctrl+A then K to exit"
echo "=========================================="
echo ""

# Configure serial port
stty -f "$PORT" $BAUD raw -echo -echoe -echok

# Monitor serial output
screen "$PORT" $BAUD
