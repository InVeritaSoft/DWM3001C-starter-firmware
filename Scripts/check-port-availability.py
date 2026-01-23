#!/usr/bin/env python3
"""
Check COM port availability and diagnose port access issues
Usage: python check-port-availability.py [COM_PORT]
"""

import sys
import serial
import serial.tools.list_ports
from pathlib import Path

def list_available_ports():
    """List all available COM ports"""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No COM ports found")
        return []
    
    print(f"\n{'='*60}")
    print("Available COM Ports:")
    print(f"{'='*60}")
    
    port_list = []
    for port in sorted(ports, key=lambda p: p.device):
        port_list.append(port.device)
        print(f"  {port.device:10} - {port.description}")
        if port.manufacturer:
            print(f"                Manufacturer: {port.manufacturer}")
        if port.hwid:
            print(f"                Hardware ID: {port.hwid}")
        print()
    
    return port_list

def check_port_access(port_name: str):
    """Check if a specific port can be opened"""
    print(f"\n{'='*60}")
    print(f"Checking Port: {port_name}")
    print(f"{'='*60}\n")
    
    # Check if port exists
    available_ports = [p.device for p in serial.tools.list_ports.comports()]
    
    if port_name not in available_ports:
        print(f"❌ Port {port_name} NOT FOUND")
        print(f"\nAvailable ports: {', '.join(available_ports) if available_ports else 'None'}")
        return False
    
    print(f"✓ Port {port_name} exists")
    
    # Try to open the port
    try:
        print(f"Attempting to open {port_name}...")
        ser = serial.Serial(
            port=port_name,
            baudrate=57600,
            timeout=1.0
        )
        print(f"✓ Port {port_name} opened successfully")
        ser.close()
        print(f"✓ Port {port_name} closed successfully")
        print(f"\n✅ Port {port_name} is AVAILABLE")
        return True
        
    except serial.SerialException as e:
        error_str = str(e)
        print(f"❌ Port {port_name} is LOCKED or IN USE")
        print(f"\nError: {error_str}")
        
        if 'PermissionError' in error_str or 'Access is denied' in error_str or 'could not open port' in error_str.lower():
            print(f"\n{'='*60}")
            print("Troubleshooting Steps:")
            print(f"{'='*60}")
            print("1. Close any serial monitors:")
            print("   - PuTTY, Tera Term, Arduino IDE Serial Monitor")
            print("   - Any PowerShell scripts running serial-monitor.ps1")
            print("   - Any Python scripts monitoring the port")
            print()
            print("2. Close other programs using the port:")
            print("   - Check Task Manager for processes using serial ports")
            print("   - Close any orchestrator web server instances")
            print("   - Close any other test scripts")
            print()
            print("3. Try unplugging and replugging the USB-to-RS485 adapter")
            print()
            print("4. Restart your computer if the port remains locked")
            print()
            print("5. Check all open terminal/PowerShell windows:")
            print("   - Look for serial monitor scripts running")
            print("   - Press Ctrl+C in any terminal that might be using the port")
        
        return False
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return False

def load_env_ports():
    """Load port configuration from .env.win"""
    env_file = Path(__file__).parent.parent / "Orchestrator" / ".env.win"
    ports = {}
    
    if env_file.exists():
        with open(env_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip()
                    if 'PORT' in key:
                        ports[key] = value
    
    return ports

def main():
    """Main entry point"""
    print("COM Port Availability Checker")
    print("="*60)
    
    # List all available ports
    available_ports = list_available_ports()
    
    # Load configured ports from .env.win
    env_ports = load_env_ports()
    if env_ports:
        print(f"\n{'='*60}")
        print("Configured Ports (from .env.win):")
        print(f"{'='*60}")
        for key, value in sorted(env_ports.items()):
            print(f"  {key}: {value}")
    
    # Check specific port if provided
    if len(sys.argv) > 1:
        port_name = sys.argv[1].upper()
        if not port_name.startswith('COM'):
            port_name = f"COM{port_name}"
        check_port_access(port_name)
    else:
        # Check configured ports
        if env_ports:
            print(f"\n{'='*60}")
            print("Checking Configured Ports:")
            print(f"{'='*60}")
            
            for key, port_name in sorted(env_ports.items()):
                if port_name:
                    print(f"\n{key}:")
                    check_port_access(port_name)
        else:
            print(f"\n{'='*60}")
            print("Usage:")
            print(f"{'='*60}")
            print("  python check-port-availability.py [COM_PORT]")
            print()
            print("Examples:")
            print("  python check-port-availability.py COM20")
            print("  python check-port-availability.py COM21")
            print()
            print("Or check configured ports from .env.win:")
            print("  python check-port-availability.py")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
