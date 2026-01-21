#!/bin/bash
# Capture serial output and send test commands
PORT="${1:-/dev/cu.usbmodem0007602011681}"
BAUD=115200
OUTPUT_FILE="/tmp/arduino_serial_$(date +%s).txt"
LOG_FILE="/Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/.cursor/debug.log"

echo "=========================================="
echo "Capturing Arduino Serial Output"
echo "=========================================="
echo "Port: $PORT"
echo "Baud: $BAUD"
echo "Output: $OUTPUT_FILE"
echo "Log: $LOG_FILE"
echo "=========================================="
echo ""

# Clear log file
> "$LOG_FILE"

# Configure serial port
stty -f "$PORT" $BAUD raw -echo

# Function to log to NDJSON
log_entry() {
    local msg="$1"
    local data="$2"
    echo "{\"timestamp\":$(date +%s)000,\"location\":\"capture-serial.sh\",\"message\":\"$msg\",\"data\":$data}" >> "$LOG_FILE"
}

log_entry "Test started" "{\"port\":\"$PORT\",\"baud\":$BAUD}"

# Read serial output in background and write to both files
{
    # Wait a bit for startup messages
    sleep 2
    
    # Send 'T' to enable test mode
    echo -n "T" > "$PORT"
    log_entry "Command sent" "{\"command\":\"T\"}"
    sleep 0.5
    
    # Monitor for 30 seconds
    end_time=$(($(date +%s) + 30))
    while [ $(date +%s) -lt $end_time ]; do
        if read -t 0.5 line < "$PORT" 2>/dev/null; then
            echo "$line" | tee -a "$OUTPUT_FILE"
            log_entry "Serial output" "{\"line\":\"$line\"}"
        fi
    done
    
    log_entry "Test completed" "{}"
} &

READ_PID=$!

# Also capture raw output using cat in parallel
cat "$PORT" >> "$OUTPUT_FILE" 2>&1 &
CAT_PID=$!

# Wait for read process
wait $READ_PID 2>/dev/null

# Kill cat process
kill $CAT_PID 2>/dev/null

echo ""
echo "=========================================="
echo "Capture complete!"
echo "Output saved to: $OUTPUT_FILE"
echo "Logs saved to: $LOG_FILE"
echo "=========================================="
echo ""
echo "Last 50 lines of output:"
echo "------------------------------------------"
tail -50 "$OUTPUT_FILE"
