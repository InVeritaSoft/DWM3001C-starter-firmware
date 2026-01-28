"""
Test Runner
Orchestrates test sequence per main.md section 5.1
"""

import asyncio
import sys
from typing import Optional, Dict, Any, List, Callable
from .node_controller import NodeController, NodeState
from .csv_logger import CSVLogger
from .config import Config


class TestRunner:
    """Test Runner - orchestrates test sequence"""
    
    def __init__(self, node_a: NodeController, node_b: NodeController, 
                 csv_logger: CSVLogger, config: Config):
        self.node_a = node_a  # TX initiator
        self.node_b = node_b  # RX logger
        self.csv_logger = csv_logger
        self.config = config
        self.is_running = False
        self.current_test: Optional[Dict[str, Any]] = None
        self.poll_task: Optional[asyncio.Task] = None
        self.poll_interval_ms = (config.get_test_config().get('poll_interval_seconds', 2)) * 1000
        self.is_polling = False  # Guard to prevent overlapping polls
        self.skip_prompts = False  # Flag to skip interactive prompts (for web API)
        
        # Callbacks
        self._test_started_callbacks: List[Callable] = []
        self._test_stopped_callbacks: List[Callable] = []
        self._configuring_callbacks: List[Callable] = []
        self._configured_callbacks: List[Callable] = []
        self._starting_callbacks: List[Callable] = []
        self._started_callbacks: List[Callable] = []
        self._stopping_callbacks: List[Callable] = []
        self._stopped_callbacks: List[Callable] = []
        self._stats_callbacks: List[Callable] = []
        self._error_callbacks: List[Callable] = []
        
        # Check if running in interactive mode
        if not sys.stdin.isatty():
            self.skip_prompts = True
    
    async def run_test(self, test: Dict[str, Any]) -> None:
        """
        Run test from test plan
        
        Args:
            test: Test configuration dict
        """
        if self.is_running:
            raise Exception("Test is already running")
        
        self.current_test = test
        self.is_running = True
        self._emit('test_started', test)
        
        try:
            # Step 1: Prompt operator to set jammer
            await self._prompt_jammer(test.get('jammer_label', 'none'))
            
            # Step 2: Configure both nodes
            await self.configure_nodes(test['config'])
            
            # Step 3: Create CSV log file
            self.csv_logger.create_log_file(test['run_name'])
            
            # Step 4: Start both nodes
            await self.start_nodes()
            
            # Step 5: Start periodic polling
            await self._start_polling(test)
            
            # Step 6: Wait indefinitely until manually stopped
            # Test will continue running until stop_test() is called
            await self._wait_until_stopped()
            
        except Exception as error:
            self._emit('error', error)
            await self.stop_test()
            raise
    
    async def _prompt_jammer(self, jammer_label: str) -> None:
        """Prompt operator to set jammer"""
        if self.skip_prompts or not sys.stdin.isatty():
            print(f"Jammer should be set to: {jammer_label} (skipping prompt - web API mode)")
            return
        
        # Interactive prompt
        print(f"\nSet jammer to: {jammer_label} and press ENTER to continue...")
        await asyncio.get_event_loop().run_in_executor(None, sys.stdin.readline)
        print(f"Jammer set to: {jammer_label}")
    
    async def configure_nodes(self, uwb_config: Dict[str, Any]) -> None:
        """Configure both nodes simultaneously"""
        self._emit('configuring')
        
        try:
            print("\n" + "=" * 60)
            print("CONFIGURING BOTH NODES SIMULTANEOUSLY")
            print("=" * 60)
            print(f"Node A state: {self.node_a.get_state()}, connected: {self.node_a.is_connected()}")
            print(f"Node B state: {self.node_b.get_state()}, connected: {self.node_b.is_connected()}")
            
            # Configure both nodes simultaneously
            results = await asyncio.gather(
                self._configure_node_a(uwb_config),
                self._configure_node_b(uwb_config),
                return_exceptions=True
            )
            
            # Check results
            errors = []
            if isinstance(results[0], Exception):
                errors.append(f"Node A: {results[0]}")
                print(f"✗ Node A configuration failed: {results[0]}")
            else:
                print("✓ Node A configured successfully")
            
            if isinstance(results[1], Exception):
                errors.append(f"Node B: {results[1]}")
                print(f"✗ Node B configuration failed: {results[1]}")
            else:
                print("✓ Node B configured successfully")
            
            if errors:
                error_msg = f"Failed to configure nodes: {'; '.join(errors)}"
                print(error_msg)
                print("=" * 60 + "\n")
                raise Exception(error_msg)
            
            print("=" * 60 + "\n")
            self._emit('configured', uwb_config)
            
        except Exception as error:
            self._emit('error', error)
            raise
    
    async def _configure_node_a(self, uwb_config: Dict[str, Any]) -> None:
        """Configure Node A (helper)"""
        await self.node_a.configure(uwb_config)
    
    async def _configure_node_b(self, uwb_config: Dict[str, Any]) -> None:
        """Configure Node B (helper)"""
        await self.node_b.configure(uwb_config)
    
    async def start_nodes(self) -> None:
        """Start both nodes simultaneously"""
        self._emit('starting')
        
        try:
            print("Starting both nodes simultaneously...")
            print(f"Node A state: {self.node_a.get_state()}, connected: {self.node_a.is_connected()}")
            print(f"Node B state: {self.node_b.get_state()}, connected: {self.node_b.is_connected()}")
            
            print("\n" + "=" * 60)
            print("SENDING START COMMANDS TO BOTH NODES SIMULTANEOUSLY")
            print("=" * 60)
            
            # Start both nodes simultaneously
            results = await asyncio.gather(
                self._start_node_a(),
                self._start_node_b(),
                return_exceptions=True
            )
            
            print("=" * 60 + "\n")
            
            # Check results
            errors = []
            if isinstance(results[0], Exception):
                errors.append(f"Node A: {results[0]}")
                print(f"✗ Node A start failed: {results[0]}")
            else:
                print("✓ Node A started successfully")
            
            if isinstance(results[1], Exception):
                errors.append(f"Node B: {results[1]}")
                print(f"✗ Node B start failed: {results[1]}")
            else:
                print("✓ Node B started successfully")
            
            if errors:
                error_msg = f"Failed to start nodes: {'; '.join(errors)}"
                print(error_msg)
                raise Exception(error_msg)
            
            self._emit('started')
            
        except Exception as error:
            self._emit('error', error)
            raise
    
    async def _start_node_a(self) -> None:
        """Start Node A (helper)"""
        await self.node_a.start_test()
    
    async def _start_node_b(self) -> None:
        """Start Node B (helper)"""
        await self.node_b.start_test()
    
    async def _start_polling(self, test: Dict[str, Any]) -> None:
        """Start periodic polling"""
        self._emit('polling_started')
        
        async def poll_loop():
            while self.is_running:
                # Prevent overlapping polls
                if self.is_polling:
                    await asyncio.sleep(self.poll_interval_ms / 1000.0)
                    continue
                
                self.is_polling = True
                try:
                    await self._poll_and_log(test)
                except Exception as error:
                    self._emit('error', error)
                finally:
                    self.is_polling = False
                
                await asyncio.sleep(self.poll_interval_ms / 1000.0)
        
        self.poll_task = asyncio.create_task(poll_loop())
    
    async def _poll_and_log(self, test: Dict[str, Any]) -> None:
        """Poll both nodes and log data"""
        try:
            # Get stats from both nodes
            stats_a = await self.node_a.get_stats()
            stats_b = await self.node_b.get_stats()
            
            # Get current configuration
            config_a = self.node_a.get_config()
            config_b = self.node_b.get_config()
            
            # Log Node A data (TX node)
            if stats_a:
                await self.csv_logger.log_data({
                    'timestamp': datetime.now().isoformat(),
                    'run_name': test['run_name'],
                    'node_id': 'A',
                    'link_distance_m': test.get('d_link', 0),
                    'env_type': test.get('env_type', ''),
                    'channel': config_a.get('channel', test['config']['channel']) if config_a else test['config']['channel'],
                    'data_rate': config_a.get('data_rate', test['config']['data_rate']) if config_a else test['config']['data_rate'],
                    'tx_power_idx': config_a.get('tx_power_idx', test['config']['tx_power_idx']) if config_a else test['config']['tx_power_idx'],
                    'pkt_rate_hz': config_a.get('pkt_rate_hz', test['config']['pkt_rate_hz']) if config_a else test['config']['pkt_rate_hz'],
                    'jammer_label': test.get('jammer_label', ''),
                    'total_rx': stats_a.get('total_sent', 0),  # Map total_sent to total_rx for CSV
                    'lost_pkts': 0,
                    'crc_err': stats_a.get('last_error', 0),
                    'rssi_avg_dbm': 0,
                    'snr_avg_db': 0,
                    'preamble_q_avg': 0,
                })
            
            # Log Node B data (RX node)
            if stats_b:
                await self.csv_logger.log_data({
                    'timestamp': datetime.now().isoformat(),
                    'run_name': test['run_name'],
                    'node_id': 'B',
                    'link_distance_m': test.get('d_link', 0),
                    'env_type': test.get('env_type', ''),
                    'channel': config_b.get('channel', test['config']['channel']) if config_b else test['config']['channel'],
                    'data_rate': config_b.get('data_rate', test['config']['data_rate']) if config_b else test['config']['data_rate'],
                    'tx_power_idx': config_b.get('tx_power_idx', test['config']['tx_power_idx']) if config_b else test['config']['tx_power_idx'],
                    'pkt_rate_hz': config_b.get('pkt_rate_hz', test['config']['pkt_rate_hz']) if config_b else test['config']['pkt_rate_hz'],
                    'jammer_label': test.get('jammer_label', ''),
                    'total_rx': stats_b.get('total_rx', 0),
                    'lost_pkts': stats_b.get('lost_pkts', 0),
                    'crc_err': stats_b.get('crc_err', 0),
                    'rssi_avg_dbm': stats_b.get('rssi_avg_dbm', 0),
                    'snr_avg_db': stats_b.get('snr_avg_db', 0),
                    'preamble_q_avg': stats_b.get('preamble_q_avg', 0),
                })
            
            # Emit stats for real-time updates
            self._emit('stats', {'nodeA': stats_a, 'nodeB': stats_b})
            
        except Exception as error:
            self._emit('error', error)
    
    async def _wait_until_stopped(self) -> None:
        """Wait until test is stopped (manually)"""
        while self.is_running:
            await asyncio.sleep(0.1)  # Check every 100ms
    
    async def stop_test(self) -> None:
        """Stop test"""
        if not self.is_running:
            return
        
        self.is_running = False
        self._emit('stopping')
        
        # Stop polling
        if self.poll_task:
            self.poll_task.cancel()
            try:
                await self.poll_task
            except asyncio.CancelledError:
                pass
            self.poll_task = None
        
        try:
            # Stop both nodes simultaneously
            print("Stopping both nodes simultaneously...")
            print(f"Node A state: {self.node_a.get_state()}, connected: {self.node_a.is_connected()}")
            print(f"Node B state: {self.node_b.get_state()}, connected: {self.node_b.is_connected()}")
            
            results = await asyncio.gather(
                self._stop_node_a(),
                self._stop_node_b(),
                return_exceptions=True
            )
            
            # Check results
            errors = []
            if isinstance(results[0], Exception):
                errors.append(f"Node A: {results[0]}")
                print(f"✗ Node A stop failed: {results[0]}")
            else:
                print("✓ Node A stopped successfully")
            
            if isinstance(results[1], Exception):
                errors.append(f"Node B: {results[1]}")
                print(f"✗ Node B stop failed: {results[1]}")
            else:
                print("✓ Node B stopped successfully")
            
            if errors:
                print(f"Some nodes failed to stop: {'; '.join(errors)}")
                # Don't throw - we still want to emit stopped event
            
            self._emit('stopped')
            self._emit('test_stopped')  # Also emit test_stopped for web server compatibility
            
        except Exception as error:
            self._emit('error', error)
            raise
    
    async def _stop_node_a(self) -> None:
        """Stop Node A (helper)"""
        await self.node_a.stop_test()
    
    async def _stop_node_b(self) -> None:
        """Stop Node B (helper)"""
        await self.node_b.stop_test()
    
    async def run_test_plan(self, tests: List[Dict[str, Any]]) -> None:
        """
        Run test plan (multiple tests)
        
        Args:
            tests: List of test configuration dicts
        """
        for i, test in enumerate(tests):
            print(f"\n=== Running test {i + 1}/{len(tests)}: {test['run_name']} ===")
            
            try:
                await self.run_test(test)
                print(f"Test {test['run_name']} completed successfully")
            except Exception as error:
                print(f"Test {test['run_name']} failed: {error}")
                self._emit('test_failed', {'test': test, 'error': error})
            
            # Wait between tests
            if i < len(tests) - 1:
                print("Waiting 5 seconds before next test...")
                await asyncio.sleep(5.0)
    
    def _emit(self, event: str, *args) -> None:
        """Emit event to callbacks"""
        callbacks_map = {
            'test_started': self._test_started_callbacks,
            'test_stopped': self._test_stopped_callbacks,
            'configuring': self._configuring_callbacks,
            'configured': self._configured_callbacks,
            'starting': self._starting_callbacks,
            'started': self._started_callbacks,
            'stopping': self._stopping_callbacks,
            'stopped': self._stopped_callbacks,
            'stats': self._stats_callbacks,
            'error': self._error_callbacks,
        }
        
        callbacks = callbacks_map.get(event, [])
        for callback in callbacks:
            try:
                callback(*args)
            except Exception as e:
                print(f"[TestRunner] Callback error for {event}: {e}")
    
    def on(self, event: str, callback: Callable) -> None:
        """Register event callback"""
        callbacks_map = {
            'test_started': self._test_started_callbacks,
            'test_stopped': self._test_stopped_callbacks,
            'configuring': self._configuring_callbacks,
            'configured': self._configured_callbacks,
            'starting': self._starting_callbacks,
            'started': self._started_callbacks,
            'stopping': self._stopping_callbacks,
            'stopped': self._stopped_callbacks,
            'stats': self._stats_callbacks,
            'error': self._error_callbacks,
        }
        
        if event in callbacks_map:
            callbacks_map[event].append(callback)


# Import datetime for timestamps
from datetime import datetime
