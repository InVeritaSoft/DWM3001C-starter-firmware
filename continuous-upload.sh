#!/bin/bash
# Continuous upload attempts with various methods
PORT="$1"
FQBN="${2:-arduino:avr:uno}"
SKETCH="${3:-RS485_Bridge.ino}"
MAX_ATTEMPTS=20

cd /Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/Arduino/RS485_Bridge

echo "=========================================="
echo "Continuous Upload Attempts"
echo "=========================================="
echo "Port: $PORT"
echo "Board: $FQBN"
echo "Max attempts: $MAX_ATTEMPTS"
echo "=========================================="
echo ""

for attempt in $(seq 1 $MAX_ATTEMPTS); do
    echo "[Attempt $attempt/$MAX_ATTEMPTS]"
    
    # Close any open connections
    pkill -f "screen.*$PORT|cu.*$PORT" 2>/dev/null
    sleep 0.2
    
    # Try different reset timings
    case $((attempt % 4)) in
        0) delay=0.3 ;;
        1) delay=0.5 ;;
        2) delay=0.7 ;;
        3) delay=0.4 ;;
    esac
    
    # Reset via baud rate change
    stty -f "$PORT" 1200 2>/dev/null
    sleep $delay
    stty -f "$PORT" 115200 2>/dev/null
    sleep $((delay + 0.3))
    
    # Attempt upload
    if arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH" 2>&1 | tee /tmp/upload_log.txt | grep -qE "(successfully|bytes.*uploaded|verified)"; then
        echo "✅✅✅ SUCCESS! Upload completed on attempt $attempt ✅✅✅"
        cat /tmp/upload_log.txt | grep -E "(Uploading|bytes|successfully|verified)"
        exit 0
    fi
    
    # Check for different error
    if grep -qE "(not in sync|programmer is not responding)" /tmp/upload_log.txt; then
        echo "  Bootloader not responding, retrying..."
    else
        echo "  Different error, checking..."
        tail -3 /tmp/upload_log.txt
    fi
    
    sleep 1
done

echo ""
echo "❌ Failed after $MAX_ATTEMPTS attempts"
echo "The board likely needs a manual reset button press"
exit 1
