"""
Node Controller
High-level control for UWB nodes (Node A = TX, Node B = RX)
"""

import asyncio
from enum import Enum
from typing import Optional, Dict, Any
from .rs485_comm import RS485Comm


class NodeState(str, Enum):
    """Node States"""
    IDLE = "IDLE"
    CONFIGURED = "CONFIGURED"
    RUNNING = "RUNNING"
    STOPPED = "STOPPED"
    ERROR = "ERROR"


class NodeController:
    """Node Controller for UWB nodes"""
    
    def __init__(self, node_id: str, rs485_comm: RS485Comm):
        self.node_id = node_id  # 'A' or 'B'
        self.rs485_comm = rs485_comm
        self.state = NodeState.IDLE
        self.config: Optional[Dict[str, Any]] = None
        self.stats: Optional[Dict[str, Any]] = None
        self.last_error: Optional[str] = None
        self._state_callbacks = []
        self._error_callbacks = []
    
    def set_state(self, new_state: NodeState) -> None:
        """Set node state"""
        if self.state != new_state:
            old_state = self.state
            self.state = new_state
            
            # Notify callbacks
            for callback in self._state_callbacks:
                try:
                    callback(old_state, new_state, self.node_id)
                except Exception as e:
                    print(f"[NodeController {self.node_id}] State callback error: {e}")
    
    def get_state(self) -> NodeState:
        """Get current state"""
        return self.state
    
    def is_connected(self) -> bool:
        """Check if node is connected"""
        return self.rs485_comm.is_open
    
    async def connect(self) -> bool:
        """Open connection to node"""
        try:
            await self.rs485_comm.open()
            return True
        except Exception as error:
            self.set_state(NodeState.ERROR)
            self.last_error = str(error)
            raise
    
    async def disconnect(self) -> bool:
        """Close connection to node"""
        try:
            await self.rs485_comm.close()
            self.set_state(NodeState.IDLE)
            return True
        except Exception as error:
            self.last_error = str(error)
            raise
    
    async def ping(self) -> bool:
        """Ping node to check connectivity"""
        try:
            # Use longer timeout for initial PING
            response = await self.rs485_comm.send_command("PNG", 3000)
            return response.startswith("OK")
        except Exception as error:
            self.last_error = str(error)
            print(f"⚠️  {self.node_id} PING failed - firmware may not be responding")
            print(f"   Error: {str(error).split(chr(10))[0]}")
            if 'timeout' in str(error).lower():
                print(f"   No response received. Check:")
                print(f"   1. Firmware is running orchestrator example")
                print(f"   2. RS-485 hardware connection")
                print(f"   3. Serial port {self.rs485_comm.port} is correct")
            return False
    
    async def get_firmware_node_type(self) -> Optional[str]:
        """Get firmware node type"""
        try:
            response = await self.rs485_comm.get_node_type()
            if response and response.startswith("OK NODE_TYPE="):
                node_type = response.split("=")[1].strip()
                return node_type
            return None
        except Exception as error:
            self.last_error = str(error)
            return None
    
    async def configure(self, config: Dict[str, Any]) -> bool:
        """Configure node with UWB settings"""
        try:
            print(f"[NodeController {self.node_id}] configure() called - state: {self.state}, connected: {self.is_connected()}")
            
            if self.state == NodeState.RUNNING:
                raise Exception("Cannot configure while test is running")
            
            if not self.is_connected():
                error_msg = f"Node {self.node_id} is not connected. Call connect() first."
                print(f"[NodeController {self.node_id}] {error_msg}")
                raise Exception(error_msg)
            
            # Quick connectivity check - send PING to verify firmware is responding
            print(f"[NodeController {self.node_id}] Sending PING to verify connectivity...")
            try:
                await self.rs485_comm.ping()
                print(f"[NodeController {self.node_id}] ✓ PING successful")
            except Exception as ping_error:
                error_msg = (
                    f"Node {self.node_id} is not responding to commands. "
                    f"Port is open but firmware may not be running or RS-485 communication is failing. "
                    f"Original error: {ping_error}"
                )
                print(f"[NodeController {self.node_id}] {error_msg}")
                raise Exception(error_msg)
            
            print(f"[NodeController {self.node_id}] Sending configuration...")
            response = await self.rs485_comm.set_config(config)
            print(f"[NodeController {self.node_id}] Configuration response: {response}")
            
            if response.startswith("OK CONFIG"):
                self.config = config.copy()
                self.set_state(NodeState.CONFIGURED)
                print(f"[NodeController {self.node_id}] ✓ Configuration successful")
                return True
            else:
                error_msg = f"Configuration failed: {response}"
                print(f"[NodeController {self.node_id}] {error_msg}")
                raise Exception(error_msg)
                
        except Exception as error:
            print(f"[NodeController {self.node_id}] configure() error: {error}")
            self.set_state(NodeState.ERROR)
            self.last_error = str(error)
            for callback in self._error_callbacks:
                try:
                    callback(error)
                except:
                    pass
            raise
    
    async def start_test(self) -> bool:
        """Start test"""
        try:
            print(f"[NodeController {self.node_id}] startTest() called - state: {self.state}, connected: {self.is_connected()}")
            
            if self.state not in [NodeState.CONFIGURED, NodeState.STOPPED]:
                error_msg = f"Cannot start test from state: {self.state} (must be CONFIGURED or STOPPED)"
                print(f"[NodeController {self.node_id}] {error_msg}")
                raise Exception(error_msg)
            
            if not self.is_connected():
                error_msg = f"Node {self.node_id} is not connected"
                print(f"[NodeController {self.node_id}] {error_msg}")
                raise Exception(error_msg)
            
            print(f"[NodeController {self.node_id}] Sending START command...")
            response = await self.rs485_comm.start_test()
            print(f"[NodeController {self.node_id}] START response: {response}")
            
            if response.startswith("OK START"):
                self.set_state(NodeState.RUNNING)
                print(f"[NodeController {self.node_id}] ✓ Test started successfully")
                return True
            else:
                error_msg = f"Start test failed: {response}"
                print(f"[NodeController {self.node_id}] {error_msg}")
                raise Exception(error_msg)
                
        except Exception as error:
            print(f"[NodeController {self.node_id}] startTest() error: {error}")
            self.set_state(NodeState.ERROR)
            self.last_error = str(error)
            for callback in self._error_callbacks:
                try:
                    callback(error)
                except:
                    pass
            raise
    
    async def stop_test(self) -> bool:
        """Stop test"""
        try:
            if self.state != NodeState.RUNNING:
                # Allow stopping from any state
                return True
            
            response = await self.rs485_comm.stop_test()
            if response.startswith("OK STOP"):
                self.set_state(NodeState.STOPPED)
                return True
            else:
                raise Exception(f"Stop test failed: {response}")
                
        except Exception as error:
            self.set_state(NodeState.ERROR)
            self.last_error = str(error)
            for callback in self._error_callbacks:
                try:
                    callback(error)
                except:
                    pass
            raise
    
    async def get_stats(self) -> Dict[str, Any]:
        """Get statistics from node"""
        try:
            response = await self.rs485_comm.get_stats()
            if response.startswith("OK STATS"):
                self.stats = self._parse_stats(response)
                return self.stats
            else:
                raise Exception(f"Get stats failed: {response}")
        except Exception as error:
            self.last_error = str(error)
            raise
    
    def _parse_stats(self, response: str) -> Dict[str, Any]:
        """
        Parse stats from GET_STATS response
        Format (Node B): OK STATS total_rx=100 lost_pkts=5 crc_err=2 rssi_avg=-65 snr_avg=25 pre_q_avg=128
        Format (Node A): OK STATS total_sent=100 last_error=0
        """
        stats = {}
        parts = response.split(" ")
        
        for part in parts[2:]:  # Skip "OK STATS"
            if "=" in part:
                key, value = part.split("=", 1)
                try:
                    # Try to convert to number
                    num_value = float(value)
                    stats[key] = int(num_value) if num_value.is_integer() else num_value
                except ValueError:
                    stats[key] = value
        
        # Node A (TX) stats
        if self.node_id == "A":
            return {
                'total_sent': stats.get('total_sent', stats.get('sent', 0)),
                'last_error': stats.get('last_error', 0),
            }
        
        # Node B (RX) stats
        # Handle case where Node B firmware might be built as Node A type
        if 'total_sent' in stats and 'total_rx' not in stats:
            print(f"[NodeController] Node B returned total_sent instead of total_rx - firmware may be built as Node A type!")
            print(f"[NodeController] Response was: {response}")
            return {
                'total_rx': 0,
                'lost_pkts': 0,
                'crc_err': 0,
                'rssi_avg_dbm': 0,
                'snr_avg_db': 0,
                'preamble_q_avg': 0,
            }
        
        return {
            'total_rx': stats.get('total_rx', stats.get('rx', 0)),
            'lost_pkts': stats.get('lost_pkts', stats.get('lost', 0)),
            'crc_err': stats.get('crc_err', 0),
            'rssi_avg_dbm': stats.get('rssi_avg', 0),
            'snr_avg_db': stats.get('snr_avg', 0),
            'preamble_q_avg': stats.get('pre_q_avg', 0),
        }
    
    async def reset_stats(self) -> bool:
        """Reset statistics"""
        try:
            response = await self.rs485_comm.reset_stats()
            if response.startswith("OK"):
                self.stats = None
                return True
            else:
                raise Exception(f"Reset stats failed: {response}")
        except Exception as error:
            self.last_error = str(error)
            raise
    
    def get_config(self) -> Optional[Dict[str, Any]]:
        """Get current configuration"""
        return self.config
    
    def get_stats_sync(self) -> Optional[Dict[str, Any]]:
        """Get current statistics (synchronous)"""
        return self.stats
    
    def get_last_error(self) -> Optional[str]:
        """Get last error"""
        return self.last_error
    
    def on_state_change(self, callback) -> None:
        """Register state change callback"""
        self._state_callbacks.append(callback)
    
    def on_error(self, callback) -> None:
        """Register error callback"""
        self._error_callbacks.append(callback)
