#!/bin/bash
# Simple serial test using basic Unix tools
PORT="${1:-/dev/cu.usbmodem0007602011681}"
BAUD=115200

echo "Testing Arduino serial communication on $PORT"
echo "Press Ctrl+C to stop"
echo ""

# Configure port
stty -f "$PORT" $BAUD raw -echo

# Send 'T' to enable test mode
echo -n "T" > "$PORT"
sleep 0.5

# Monitor output using cat (simple but effective)
cat "$PORT"
