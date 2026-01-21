#!/bin/bash
# Test Arduino to DWM communication
# Usage: ./test-arduino-dwm.sh [port]
# Example: ./test-arduino-dwm.sh /dev/cu.usbmodem0007602011681

PORT="${1:-/dev/cu.usbmodem0007602011681}"
BAUD=115200
LOG_FILE="/Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/.cursor/debug.log"

echo "=========================================="
echo "Arduino to DWM Communication Test"
echo "=========================================="
echo "Port: $PORT"
echo "Baud: $BAUD"
echo "Log file: $LOG_FILE"
echo "=========================================="
echo ""

# Configure serial port
stty -f "$PORT" $BAUD raw -echo -echoe -echok

# Function to send command via serial
send_command() {
    echo -n "$1" > "$PORT"
    sleep 0.1
}

# Function to read serial output with timeout
read_serial() {
    timeout=5
    start=$(date +%s)
    output=""
    while [ $(($(date +%s) - start)) -lt $timeout ]; do
        if read -t 0.1 line < "$PORT" 2>/dev/null; then
            output="$output$line"
            echo "$line"
            # Also log to file
            echo "{\"timestamp\":$(date +%s)000,\"location\":\"test-arduino-dwm.sh\",\"message\":\"Serial output\",\"data\":{\"line\":\"$line\"}}" >> "$LOG_FILE"
        fi
    done
}

# Clear log file
> "$LOG_FILE"

echo "[TEST] Waiting for Arduino startup messages..."
sleep 2

echo "[TEST] Sending 'T' to enable test mode..."
send_command "T"

echo "[TEST] Monitoring output for 30 seconds..."
echo "=========================================="
read_serial

# Keep monitoring
for i in {1..6}; do
    sleep 5
    echo ""
    echo "[TEST] Status check $i/6..."
    read_serial
done

echo ""
echo "=========================================="
echo "Test complete. Check $LOG_FILE for logs."
echo "=========================================="
