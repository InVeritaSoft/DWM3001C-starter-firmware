# Arduino Flash and Test Instructions

## Current Status

**Issue:** Automated upload is failing because Arduino boards require a manual reset button press to enter bootloader mode. This is a hardware limitation that cannot be bypassed programmatically.

## Manual Upload Required (One-Time Setup)

1. **Open Arduino IDE**
2. **Load sketch:** `Arduino/RS485_Bridge/RS485_Bridge.ino`
3. **Select Board:** Tools → Board → Arduino Uno
4. **Select Port:** 
   - First Arduino: `/dev/cu.usbmodem0007602011681`
   - Second Arduino: `/dev/cu.usbmodem0007602015991`
5. **Upload:** Click Upload button
6. **If upload fails:** Press and release the **RESET button** on the Arduino when "Uploading..." appears

## Automated Testing (After Upload)

Once firmware is uploaded, run:

```bash
# Test first Arduino
./monitor-and-test.sh /dev/cu.usbmodem0007602011681

# Test second Arduino  
./monitor-and-test.sh /dev/cu.usbmodem0007602015991
```

This will:
- Monitor serial output for 60 seconds
- Automatically enable test mode (sends 'T')
- Capture all debug logs
- Show Arduino→DWM communication

## Debug Logs

All logs are saved to:
- `.cursor/debug.log` - NDJSON format for analysis
- `/tmp/arduino_monitor_*.txt` - Raw serial output

## What the Debug Logs Show

The instrumentation tracks:
- Commands sent to DWM (exact bytes)
- DWM serial listener state
- First byte received from DWM
- Complete responses or timeout details
- Test commands (PNG, NODE_TYPE, STAT) sent every 5 seconds
