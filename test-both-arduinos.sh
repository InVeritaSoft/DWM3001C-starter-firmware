#!/bin/sh
# Test both Arduino boards for communication

LOG_FILE="/Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/.cursor/debug.log"
> "$LOG_FILE"

test_port() {
    PORT=$1
    echo "=========================================="
    echo "Testing: $PORT"
    echo "=========================================="
    
    # Check if port exists
    if [ ! -e "$PORT" ]; then
        echo "Port $PORT does not exist"
        return
    fi
    
    # Configure port
    stty -f "$PORT" 115200 raw -echo 2>/dev/null || {
        echo "Failed to configure port"
        return
    }
    
    echo "Port configured, waiting for output..."
    
    # Try to read with timeout
    (cat "$PORT" & PID=$!; sleep 10; kill $PID 2>/dev/null) | head -50 | while read line; do
        echo "$line"
        echo "{\"timestamp\":$(date +%s)000,\"location\":\"test-both-arduinos.sh\",\"message\":\"Serial output\",\"data\":{\"port\":\"$PORT\",\"line\":\"$line\"}}" >> "$LOG_FILE"
    done
    
    echo "Sending 'T' command..."
    echo -n "T" > "$PORT" 2>/dev/null
    sleep 1
    
    # Try reading again
    (cat "$PORT" & PID=$!; sleep 10; kill $PID 2>/dev/null) | head -50 | while read line; do
        echo "$line"
        echo "{\"timestamp\":$(date +%s)000,\"location\":\"test-both-arduinos.sh\",\"message\":\"Serial output after T\",\"data\":{\"port\":\"$PORT\",\"line\":\"$line\"}}" >> "$LOG_FILE"
    done
    
    echo ""
}

test_port "/dev/cu.usbmodem0007602011681"
test_port "/dev/cu.usbmodem0007602015991"

echo "=========================================="
echo "Test complete. Check $LOG_FILE for logs."
echo "=========================================="
