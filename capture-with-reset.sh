#!/bin/bash
# Capture serial output with Arduino reset
PORT="${1:-/dev/cu.usbmodem0007602011681}"
BAUD=115200
OUTPUT="/tmp/arduino_capture_$(date +%s).txt"
LOG_FILE="/Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/.cursor/debug.log"

echo "=========================================="
echo "Arduino Serial Capture with Reset"
echo "=========================================="
echo "Port: $PORT"
echo "Output: $OUTPUT"
echo "=========================================="

# Clear files
> "$OUTPUT"
> "$LOG_FILE"

# Function to log
log_json() {
    echo "{\"timestamp\":$(date +%s)000,\"location\":\"capture-with-reset.sh\",\"message\":\"$1\",\"data\":$2}" >> "$LOG_FILE"
}

log_json "Test started" "{\"port\":\"$PORT\"}"

# Reset Arduino by changing baud rate to 1200 (triggers bootloader/DTR reset)
echo "[1/4] Resetting Arduino..."
stty -f "$PORT" 1200 2>/dev/null
sleep 0.5
stty -f "$PORT" $BAUD raw -echo 2>/dev/null
sleep 1

echo "[2/4] Starting capture (15 seconds)..."
# Capture output
(cat "$PORT" >> "$OUTPUT" 2>&1 & CAT_PID=$!
sleep 2
echo "[3/4] Sending 'T' command..."
printf "T" > "$PORT" 2>/dev/null
log_json "Command sent" "{\"command\":\"T\"}"
sleep 13
kill $CAT_PID 2>/dev/null
wait $CAT_PID 2>/dev/null) &

CAPTURE_PID=$!
wait $CAPTURE_PID

echo "[4/4] Capture complete"
echo ""
echo "=========================================="
echo "Captured output:"
echo "=========================================="
cat "$OUTPUT"
echo ""
echo "=========================================="
echo "File saved to: $OUTPUT"
echo "Logs saved to: $LOG_FILE"
echo "=========================================="

# Log all captured lines
while IFS= read -r line; do
    if [ -n "$line" ]; then
        log_json "Serial output" "{\"line\":\"$line\"}"
    fi
done < "$OUTPUT"
