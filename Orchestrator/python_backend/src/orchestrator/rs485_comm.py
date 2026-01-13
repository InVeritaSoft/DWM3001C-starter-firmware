"""
RS-485 Communication Module
Handles serial communication with UWB nodes via RS-485
"""

import asyncio
import serial
import serial.tools.list_ports
from typing import Optional, Dict, Tuple
from dataclasses import dataclass
import time


@dataclass
class PendingCommand:
    """Pending command awaiting response"""
    command: str
    future: asyncio.Future
    timer_task: Optional[asyncio.Task] = None


class RS485Comm:
    """RS-485 Communication Handler"""
    
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 1.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial_port: Optional[serial.Serial] = None
        self.is_open = False
        self.pending_commands: Dict[int, PendingCommand] = {}
        self.command_id = 0
        self._read_task: Optional[asyncio.Task] = None
        self._lock = asyncio.Lock()
        
    async def open(self) -> None:
        """Open serial port connection"""
        try:
            print(f"[RS485] Opening {self.port} at {self.baudrate} baud...")
            
            # Create serial port
            self.serial_port = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,  # Non-blocking read with short timeout
                rtscts=False,
                dsrdtr=False,
            )
            
            self.is_open = True
            print(f"[RS485] {self.port} opened successfully")
            
            # Start background read task
            self._read_task = asyncio.create_task(self._read_loop())
            
            # Wait for firmware initialization (firmware needs time to start)
            await asyncio.sleep(2.0)
            
        except Exception as error:
            print(f"[RS485 ERROR] {self.port}: Failed to open: {error}")
            raise Exception(f"Failed to open port {self.port}: {error}")
    
    async def close(self) -> None:
        """Close serial port connection"""
        if self._read_task:
            self._read_task.cancel()
            try:
                await self._read_task
            except asyncio.CancelledError:
                pass
        
        if self.serial_port and self.is_open:
            self.serial_port.close()
            self.is_open = False
            print(f"[RS485] Port {self.port} closed")
    
    async def _read_loop(self) -> None:
        """Background task to read serial data"""
        buffer = b""
        import time
        
        print(f"[RS485 READ LOOP] {self.port}: Read loop started")
        
        while self.is_open:
            try:
                if self.serial_port:
                    # Check if data is available first (more efficient)
                    if self.serial_port.in_waiting > 0:
                        # Read available data
                        data = self.serial_port.read(self.serial_port.in_waiting)
                        timestamp = time.time()
                        
                        if len(data) > 0:
                            # Log raw data - CAPTURE EVERYTHING
                            hex_str = data.hex()
                            ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data)
                            print(f"[RS485 RAW @{timestamp:.3f}] {self.port}: {len(data)} bytes - Hex={hex_str}, ASCII=\"{ascii_str}\"")
                            
                            buffer += data
                            
                            # Process complete lines (ending with \r\n)
                            while b'\n' in buffer:
                                line, buffer = buffer.split(b'\n', 1)
                                line = line.rstrip(b'\r')
                                
                                if line:
                                    decoded = line.decode('ascii', errors='replace').strip()
                                    print(f"[RS485 LINE @{timestamp:.3f}] {self.port}: \"{decoded}\"")
                                    print(f"[RS485 LINE @{timestamp:.3f}] {self.port}: Hex: {line.hex()}, Pending: {len(self.pending_commands)}")
                                    
                                    await self._handle_response(decoded)
                            
                            # Log incomplete line if buffer has data
                            if len(buffer) > 0 and b'\n' not in buffer:
                                buffer_hex = buffer.hex()
                                buffer_ascii = ''.join(chr(b) if 32 <= b < 127 else '.' for b in buffer)
                                print(f"[RS485 PARTIAL @{timestamp:.3f}] {self.port}: Incomplete ({len(buffer)} bytes): {buffer_hex}")
                    else:
                        # No data available - sleep longer to reduce CPU usage
                        await asyncio.sleep(0.1)  # 100ms when idle
                        continue
                else:
                    await asyncio.sleep(0.1)
                
                # Small delay when processing data
                await asyncio.sleep(0.01)
                
            except serial.SerialException as e:
                print(f"[RS485 ERROR] {self.port}: Serial exception: {e}")
                await asyncio.sleep(0.1)
            except Exception as e:
                import traceback
                print(f"[RS485 ERROR] {self.port}: Read loop error: {e}")
                await asyncio.sleep(0.1)
        
        print(f"[RS485 READ LOOP] {self.port}: Read loop stopped")
    
    async def _handle_response(self, data: str) -> None:
        """Handle incoming response"""
        import time
        timestamp = time.time()
        
        # Skip empty lines
        if not data:
            return
        
        # Filter out firmware debug messages
        lower = data.lower()
        is_filtered = (
            '[uart]' in lower or
            '[cmd]' in lower or
            '[handler]' in lower or
            '[parser]' in lower or
            '[uart_tx]' in lower or
            '[dbg]' in lower or
            '<dbg>' in lower or
            '[rx]' in lower or
            'mpu:' in lower or
            'os:' in lower or
            (lower.startswith('[') and not lower.startswith('[ok') and not lower.startswith('[err'))
        )
        
        if is_filtered:
            print(f"[RS485 FILTERED] {self.port}: {data}")
            return
        
        # Find matching pending command (FIFO - match oldest command first)
        if data.upper().startswith('OK') or data.upper().startswith('ERR'):
            # Get oldest pending command
            if self.pending_commands:
                command_id = min(self.pending_commands.keys())
                pending = self.pending_commands[command_id]
                
                # Cancel timeout timer
                if pending.timer_task:
                    pending.timer_task.cancel()
                
                # Remove from pending
                del self.pending_commands[command_id]
                
                # Log successful response match
                print(f"[RS485 OK] {self.port}: Command \"{pending.command}\" → Response: {data}")
                
                # Resolve or reject
                if data.upper().startswith('OK'):
                    pending.future.set_result(data)
                else:
                    pending.future.set_exception(Exception(data))
                return
            else:
                print(f"[RS485 WARNING] {self.port}: Received OK/ERR response with no pending command: {data}")
        
        # Unsolicited data
        print(f"[RS485 WARNING] {self.port}: Received data that doesn't match OK/ERR format or has no pending command: {data}")
    
    async def _timeout_handler(self, command_id: int, command: str, timeout_ms: float) -> None:
        """Handle command timeout"""
        await asyncio.sleep(timeout_ms / 1000.0)
        
        if command_id in self.pending_commands:
            pending = self.pending_commands[command_id]
            del self.pending_commands[command_id]
            
            error_msg = (
                f"Command timeout: {command}\n"
                f"  Port: {self.port}\n"
                f"  Timeout: {timeout_ms}ms\n"
                f"  No response received from firmware.\n"
                f"\nTroubleshooting:\n"
                f"  1. Check LED behavior on board:\n"
                f"     - Orange LED should blink when command is received\n"
                f"     - Green LED should blink when response is sent\n"
                f"     - If no LEDs blink, firmware may not be receiving commands\n"
                f"  2. Verify firmware is running orchestrator v2:\n"
                f"     - Check LED behavior on startup (should blink)\n"
                f"     - Rebuild and flash: ./build-and-flash-tx.sh or ./build-and-flash-rx.sh\n"
                f"  3. Check RS-485 hardware connection:\n"
                f"     - Verify RS-485 transceiver is connected\n"
                f"     - Check wiring (A+/B- lines, GND)\n"
                f"     - Ensure proper termination resistors (120Ω at each end)\n"
                f"     - Verify transceiver enable/DE pins are configured\n"
                f"  4. Verify serial port:\n"
                f"     - Port {self.port} is correct\n"
                f"     - Baud rate matches firmware (115200)\n"
                f"     - No other software using the port\n"
                f"  5. Test with direct serial terminal:\n"
                f"     - Open serial terminal (minicom, screen, etc.)\n"
                f"     - Configure: 115200 baud, 8N1, no flow control\n"
                f"     - Send \"PNG\\r\\n\" and check for \"OK\\r\\n\" response\n"
                f"     - Watch LEDs: orange on TX, green on RX"
            )
            
            pending.future.set_exception(Exception(error_msg))
    
    async def send_command(self, command: str, timeout_ms: Optional[float] = None) -> str:
        """
        Send command and wait for response
        
        Args:
            command: ASCII command to send
            timeout_ms: Timeout in milliseconds (default: self.timeout * 1000)
            
        Returns:
            Response string
        """
        if timeout_ms is None:
            timeout_ms = self.timeout * 1000
        
        if not self.is_open:
            raise Exception(f"Port {self.port} is not open")
        
        async with self._lock:
            self.command_id += 1
            command_id = self.command_id
            command_str = f"{command}\r\n"
            
            # Create future for response
            future = asyncio.Future()
            
            # Create timeout task
            timer_task = asyncio.create_task(
                self._timeout_handler(command_id, command, timeout_ms)
            )
            
            # Store pending command
            self.pending_commands[command_id] = PendingCommand(
                command=command,
                future=future,
                timer_task=timer_task
            )
            
            # Log TX
            print(f"[RS485 TX] {self.port}: {command.strip()} ({len(command_str)} bytes including \\r\\n)")
            print(f"[RS485 TX] {self.port}: Hex: {command_str.encode().hex()}")
            
            # Write command
            try:
                self.serial_port.write(command_str.encode('ascii'))
                self.serial_port.flush()  # Ensure data is sent
            except Exception as e:
                timer_task.cancel()
                del self.pending_commands[command_id]
                raise Exception(f"Failed to write command: {e}")
            
            # Wait for response
            try:
                response = await future
                return response
            except asyncio.CancelledError:
                if command_id in self.pending_commands:
                    del self.pending_commands[command_id]
                raise
    
    async def ping(self) -> str:
        """Send PING command (short code PNG)"""
        return await self.send_command("PNG")
    
    async def get_node_type(self) -> str:
        """Get node type from firmware"""
        return await self.send_command("NODE_TYPE")
    
    async def set_config(self, config: dict) -> str:
        """
        Send SET_CONFIG command
        
        Args:
            config: Configuration dict with keys:
                - channel: UWB channel (5 or 9)
                - data_rate: Data rate ("6m8" or "850k")
                - preamble_len: Preamble length (64, 128, 256, 512, 1024)
                - payload_len: Payload length in bytes
                - tx_power_idx: TX power index
                - pkt_rate_hz: Packet rate in Hz
        """
        print(f"[RS485] setConfig() called for port {self.port} (isOpen: {self.is_open})")
        
        params = []
        if 'channel' in config:
            params.append(f"ch={config['channel']}")
        if 'data_rate' in config:
            # Handle both string and numeric data_rate values
            rate_str = config['data_rate']
            if isinstance(rate_str, int):
                rate_str = "6m8" if rate_str == 1 else "850k"
            params.append(f"rate={rate_str}")
        if 'preamble_len' in config:
            params.append(f"pl={config['preamble_len']}")
        if 'payload_len' in config:
            params.append(f"len={config['payload_len']}")
        if 'tx_power_idx' in config:
            params.append(f"pwr={config['tx_power_idx']}")
        if 'pkt_rate_hz' in config:
            params.append(f"rate_hz={config['pkt_rate_hz']}")
        
        command = f"CFG {' '.join(params)}"
        print(f"[RS485] Sending CFG command to port {self.port}: {command}")
        
        # Use longer timeout for SET_CONFIG commands
        return await self.send_command(command, 10000)
    
    async def start_test(self) -> str:
        """Send START_TEST command (short code STRT)"""
        print(f"[RS485] Sending START command to port {self.port} (isOpen: {self.is_open})")
        return await self.send_command("STRT")
    
    async def stop_test(self) -> str:
        """Send STOP_TEST command (short code STOP)"""
        return await self.send_command("STOP")
    
    async def get_stats(self) -> str:
        """Send GET_STATS command (short code STAT)"""
        return await self.send_command("STAT")
    
    async def reset_stats(self) -> str:
        """Send RESET_STATS command (short code RST)"""
        return await self.send_command("RST")
    
    async def set_log_mode(self, level: int) -> str:
        """Send SET_LOG_MODE command"""
        return await self.send_command(f"SET_LOG_MODE level={level}")
