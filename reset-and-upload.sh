#!/bin/bash
# Reset Arduino and upload firmware
PORT="$1"
FQBN="${2:-arduino:avr:uno}"
SKETCH="${3:-RS485_Bridge.ino}"

echo "=========================================="
echo "Resetting and Uploading to Arduino"
echo "=========================================="
echo "Port: $PORT"
echo "Board: $FQBN"
echo "Sketch: $SKETCH"
echo "=========================================="

# Method 1: Baud rate change to trigger bootloader
echo "[1/5] Attempting DTR reset via baud rate change..."
stty -f "$PORT" 1200 2>/dev/null
sleep 0.3
stty -f "$PORT" 115200 2>/dev/null
sleep 0.5

# Method 2: Try upload immediately after reset
echo "[2/5] Attempting upload (method 1)..."
cd /Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/Arduino/RS485_Bridge
if arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH" 2>&1 | grep -q "successfully uploaded\|bytes.*uploaded"; then
    echo "✅ Upload successful!"
    exit 0
fi

# Method 3: Try with different baud rate
echo "[3/5] Attempting upload with 57600 baud..."
HEX_FILE=$(find /Users/lolibai/Library/Caches/arduino/sketches -name "*.hex" -path "*${SKETCH}.hex" | head -1)
if [ -f "$HEX_FILE" ]; then
    /Users/lolibai/Library/Arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/bin/avrdude \
        -C/Users/lolibai/Library/Arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/etc/avrdude.conf \
        -v -patmega328p -carduino -P"$PORT" -b57600 -D \
        -Uflash:w:"$HEX_FILE":i 2>&1 | grep -q "bytes.*written\|verified" && {
        echo "✅ Upload successful with 57600 baud!"
        exit 0
    }
fi

# Method 4: Try with 19200 baud (some bootloaders use this)
echo "[4/5] Attempting upload with 19200 baud..."
if [ -f "$HEX_FILE" ]; then
    /Users/lolibai/Library/Arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/bin/avrdude \
        -C/Users/lolibai/Library/Arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/etc/avrdude.conf \
        -v -patmega328p -carduino -P"$PORT" -b19200 -D \
        -Uflash:w:"$HEX_FILE":i 2>&1 | grep -q "bytes.*written\|verified" && {
        echo "✅ Upload successful with 19200 baud!"
        exit 0
    }
fi

# Method 5: Final attempt with standard method
echo "[5/5] Final attempt with standard upload..."
stty -f "$PORT" 1200 2>/dev/null
sleep 0.5
stty -f "$PORT" 115200 2>/dev/null
sleep 1
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH" 2>&1 | grep -q "successfully uploaded\|bytes.*uploaded" && {
    echo "✅ Upload successful!"
    exit 0
}

echo "❌ All upload methods failed"
echo "The board may need a manual reset button press"
exit 1
