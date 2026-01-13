#!/usr/bin/env python3
"""
Test script to verify RS485 serial port can receive data
Run this to check if data is arriving at the serial port when RX LED blinks
"""

import serial
import sys
import time
import os

def test_port_receive(port_path: str, baudrate: int = 115200):
    """Test if we can receive data from the serial port"""
    
    print(f"Testing RS485 port: {port_path}")
    print(f"Baudrate: {baudrate}")
    print(f"Watch the RX LED on your RS485 device - it should blink when data arrives")
    print(f"Press Ctrl+C to stop\n")
    
    # Check if port exists
    if not os.path.exists(port_path):
        print(f"ERROR: Port {port_path} does not exist!")
        print(f"Available ports:")
        import serial.tools.list_ports
        for port in serial.tools.list_ports.comports():
            print(f"  - {port.device}: {port.description}")
        return False
    
    # Check permissions
    if not os.access(port_path, os.R_OK):
        print(f"ERROR: No read permission for {port_path}")
        print(f"Try: sudo chmod 666 {port_path}")
        print(f"Or add your user to dialout group: sudo usermod -a -G dialout $USER")
        return False
    
    try:
        # Open serial port
        print(f"Opening port {port_path}...")
        ser = serial.Serial(
            port=port_path,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1.0,  # 1 second timeout for blocking read
            rtscts=False,
            dsrdtr=False,
        )
        
        print(f"✓ Port opened successfully!")
        print(f"  Port name: {ser.name}")
        print(f"  Baudrate: {ser.baudrate}")
        print(f"  Timeout: {ser.timeout}")
        print(f"  Bytes in waiting: {ser.in_waiting}")
        print(f"\nMonitoring for incoming data...")
        print(f"Watch the RX LED - when it blinks, you should see data below:\n")
        
        byte_count = 0
        start_time = time.time()
        last_data_time = start_time
        
        while True:
            # Read available data
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                timestamp = time.time()
                byte_count += len(data)
                last_data_time = timestamp
                
                # Log raw data
                hex_str = data.hex()
                ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data)
                print(f"[{timestamp:.3f}] RECEIVED {len(data)} bytes:")
                print(f"  Hex: {hex_str}")
                print(f"  ASCII: \"{ascii_str}\"")
                print(f"  Bytes: [{', '.join(f'{b:02X}' for b in data)}]")
                print()
            else:
                # No data - check if we've been waiting too long
                current_time = time.time()
                if current_time - last_data_time > 10.0 and byte_count == 0:
                    print(f"[{current_time:.3f}] Waiting for data... (RX LED should blink when data arrives)")
                    last_data_time = current_time
                elif current_time - last_data_time > 30.0:
                    print(f"[{current_time:.3f}] No data received in 30 seconds. Total bytes received: {byte_count}")
                    print(f"  - If RX LED is blinking but no data here, check:")
                    print(f"    1. Port path is correct: {port_path}")
                    print(f"    2. Baudrate matches: {baudrate}")
                    print(f"    3. No other program is using the port")
                    print(f"    4. Serial port permissions")
                    last_data_time = current_time
            
            time.sleep(0.1)  # 100ms delay to reduce CPU usage
            
    except serial.SerialException as e:
        print(f"ERROR: Serial port error: {e}")
        return False
    except KeyboardInterrupt:
        print(f"\n\nStopped by user")
        print(f"Total bytes received: {byte_count}")
        if ser.is_open:
            ser.close()
        return True
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_rs485_receive.py <port_path> [baudrate]")
        print("Example: python test_rs485_receive.py /dev/ttyUSB0 115200")
        print("\nAvailable ports:")
        import serial.tools.list_ports
        for port in serial.tools.list_ports.comports():
            print(f"  - {port.device}: {port.description}")
        sys.exit(1)
    
    port_path = sys.argv[1]
    baudrate = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    
    success = test_port_receive(port_path, baudrate)
    sys.exit(0 if success else 1)
