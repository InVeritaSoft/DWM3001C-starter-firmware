#!/usr/bin/env python3
"""
Test Arduino to DWM communication
Monitors serial output and sends test commands
"""
import serial
import time
import json
import sys
from datetime import datetime

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem0007602011681"
BAUD = 115200
LOG_FILE = "/Users/lolibai/Documents/inverita/DWM3001C-starter-firmware/.cursor/debug.log"

def log_to_file(message, data=None):
    """Log to NDJSON file"""
    entry = {
        "timestamp": int(time.time() * 1000),
        "location": "test_arduino_dwm.py",
        "message": message,
        "data": data or {}
    }
    with open(LOG_FILE, "a") as f:
        f.write(json.dumps(entry) + "\n")

def main():
    print("=" * 50)
    print("Arduino to DWM Communication Test")
    print("=" * 50)
    print(f"Port: {PORT}")
    print(f"Baud: {BAUD}")
    print(f"Log: {LOG_FILE}")
    print("=" * 50)
    print()
    
    # Clear log file
    open(LOG_FILE, "w").close()
    log_to_file("Test started", {"port": PORT, "baud": BAUD})
    
    try:
        # Open serial port
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print("[TEST] Serial port opened")
        time.sleep(2)  # Wait for Arduino to initialize
        
        # Read any startup messages
        print("[TEST] Reading startup messages...")
        time.sleep(2)
        while ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
                log_to_file("Serial output", {"line": line})
        
        print()
        print("[TEST] Sending 'T' to enable test mode...")
        ser.write(b'T')
        log_to_file("Command sent", {"command": "T"})
        time.sleep(0.5)
        
        # Monitor for 30 seconds
        print("[TEST] Monitoring output for 30 seconds...")
        print("=" * 50)
        
        start_time = time.time()
        while time.time() - start_time < 30:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(line)
                    log_to_file("Serial output", {"line": line})
            else:
                time.sleep(0.1)
        
        print()
        print("=" * 50)
        print("Test complete. Check logs in:", LOG_FILE)
        print("=" * 50)
        
        ser.close()
        log_to_file("Test completed")
        
    except serial.SerialException as e:
        print(f"[ERROR] Serial port error: {e}")
        log_to_file("Error", {"error": str(e)})
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n[TEST] Interrupted by user")
        log_to_file("Test interrupted")
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    main()
