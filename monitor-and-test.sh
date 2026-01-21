#!/bin/bash
# Monitor Arduino and test DWM communication
# This script will work once firmware is uploaded

PORT="${1:-/dev/cu.usbmodem0007602011681}"
BAUD=115200
LOG_FILE="/Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/.cursor/debug.log"
OUTPUT_FILE="/tmp/arduino_monitor_$(date +%s).txt"

echo "=========================================="
echo "Arduino Serial Monitor & DWM Test"
echo "=========================================="
echo "Port: $PORT"
echo "Baud: $BAUD"
echo "Log: $LOG_FILE"
echo "Output: $OUTPUT_FILE"
echo "=========================================="
echo ""
echo "This script will:"
echo "1. Monitor serial output"
echo "2. Send 'T' to enable test mode after 3 seconds"
echo "3. Capture all output for 60 seconds"
echo "4. Save logs for analysis"
echo ""
echo "Press Ctrl+C to stop early"
echo "=========================================="
echo ""

# Clear log file
> "$LOG_FILE"
> "$OUTPUT_FILE"

# Function to log to NDJSON
log_json() {
    local msg="$1"
    local data="$2"
    local ts=$(date +%s)000
    echo "{\"timestamp\":$ts,\"location\":\"monitor-and-test.sh\",\"message\":\"$msg\",\"data\":$data}" >> "$LOG_FILE"
}

log_json "Test started" "{\"port\":\"$PORT\",\"baud\":$BAUD}"

# Configure serial port
if ! stty -f "$PORT" $BAUD raw -echo 2>/dev/null; then
    echo "ERROR: Failed to configure port $PORT"
    log_json "Error" "{\"error\":\"Failed to configure port\"}"
    exit 1
fi

echo "[INFO] Port configured, starting monitor..."
echo ""

# Background process to send 'T' after delay
(
    sleep 3
    echo -n "T" > "$PORT" 2>/dev/null
    echo "[INFO] Sent 'T' command to enable test mode"
    log_json "Command sent" "{\"command\":\"T\"}"
) &

# Monitor serial output
echo "[INFO] Monitoring serial output (60 seconds)..."
echo "------------------------------------------"

# Use a combination of approaches to capture output
{
    # Read with timeout
    end_time=$(($(date +%s) + 60))
    while [ $(date +%s) -lt $end_time ]; do
        # Try to read a line
        if IFS= read -t 0.5 -r line < "$PORT" 2>/dev/null; then
            echo "$line"
            echo "$line" >> "$OUTPUT_FILE"
            log_json "Serial output" "{\"line\":\"$line\"}"
        fi
    done
} &

READ_PID=$!

# Also use cat as backup
cat "$PORT" >> "$OUTPUT_FILE" 2>&1 &
CAT_PID=$!

# Wait for read process
wait $READ_PID 2>/dev/null

# Clean up
kill $CAT_PID 2>/dev/null 2>&1
kill %1 2>/dev/null 2>&1

echo ""
echo "------------------------------------------"
echo "[INFO] Monitoring complete"
echo ""
echo "Output saved to: $OUTPUT_FILE"
echo "Logs saved to: $LOG_FILE"
echo ""
echo "Last 100 lines of output:"
echo "=========================================="
tail -100 "$OUTPUT_FILE"
echo ""
echo "=========================================="
log_json "Test completed" "{}"
