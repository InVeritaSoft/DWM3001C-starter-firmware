#!/usr/bin/env python3
"""
Comprehensive Test Script for Both UWB Nodes
Tests all available commands on Node A (TX) and Node B (RX)

Usage:
    python test-all-commands.py
    python test-all-commands.py --node-a-port COM20 --node-b-port COM21
    python test-all-commands.py --baudrate 57600
"""

import asyncio
import sys
import argparse
from pathlib import Path
from typing import Dict, Any, Optional
from datetime import datetime

# Add orchestrator to path
orchestrator_path = Path(__file__).parent.parent / "Orchestrator" / "python_backend" / "src"
sys.path.insert(0, str(orchestrator_path))

from orchestrator.rs485_comm import RS485Comm
from orchestrator.node_controller import NodeController, NodeState


class Colors:
    """ANSI color codes for terminal output"""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


class TestResult:
    """Test result container"""
    def __init__(self, test_name: str, node_id: str):
        self.test_name = test_name
        self.node_id = node_id
        self.success = False
        self.response = None
        self.error = None
        self.duration_ms = 0


class CommandTester:
    """Comprehensive command tester for both nodes"""
    
    def __init__(self, node_a_port: str, node_b_port: str, baudrate: int = 57600):
        self.node_a_port = node_a_port
        self.node_b_port = node_b_port
        self.baudrate = baudrate
        self.node_a: Optional[NodeController] = None
        self.node_b: Optional[NodeController] = None
        self.results: list[TestResult] = []
    
    def print_header(self, text: str):
        """Print formatted header"""
        print(f"\n{Colors.BOLD}{Colors.HEADER}{'='*70}{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}{text:^70}{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}{'='*70}{Colors.ENDC}\n")
    
    def print_test(self, text: str):
        """Print test name"""
        print(f"{Colors.OKCYAN}[TEST]{Colors.ENDC} {text}")
    
    def print_success(self, text: str):
        """Print success message"""
        print(f"{Colors.OKGREEN}[✓]{Colors.ENDC} {text}")
    
    def print_error(self, text: str):
        """Print error message"""
        print(f"{Colors.FAIL}[✗]{Colors.ENDC} {text}")
    
    def print_info(self, text: str):
        """Print info message"""
        print(f"{Colors.OKBLUE}[INFO]{Colors.ENDC} {text}")
    
    async def run_test(self, test_name: str, node_id: str, test_func) -> TestResult:
        """Run a test and record result"""
        result = TestResult(test_name, node_id)
        start_time = asyncio.get_event_loop().time()
        
        self.print_test(f"{node_id}: {test_name}")
        
        try:
            response = await test_func()
            result.success = True
            result.response = response
            duration = (asyncio.get_event_loop().time() - start_time) * 1000
            result.duration_ms = duration
            self.print_success(f"{node_id}: {test_name} - {response[:80]}")
            if duration > 1000:
                self.print_info(f"Duration: {duration:.1f}ms")
        except Exception as e:
            duration = (asyncio.get_event_loop().time() - start_time) * 1000
            result.duration_ms = duration
            result.error = str(e)
            self.print_error(f"{node_id}: {test_name} - {str(e)[:80]}")
        
        self.results.append(result)
        return result
    
    async def setup(self):
        """Setup connections to both nodes"""
        self.print_header("Setting Up Connections")
        
        # Create RS485 communication objects
        rs485_a = RS485Comm(self.node_a_port, self.baudrate, timeout=5.0)
        rs485_b = RS485Comm(self.node_b_port, self.baudrate, timeout=5.0)
        
        # Create node controllers
        self.node_a = NodeController("A", rs485_a)
        self.node_b = NodeController("B", rs485_b)
        
        # Connect to both nodes
        self.print_info("Connecting to Node A...")
        try:
            await self.node_a.connect()
            self.print_success("Node A connected")
        except Exception as e:
            self.print_error(f"Failed to connect Node A: {e}")
            if "Access denied" in str(e) or "PermissionError" in str(e) or "could not open port" in str(e).lower():
                self.print_info("Tip: Run 'python Scripts/check-port-availability.py' to diagnose port issues")
            raise
        
        self.print_info("Connecting to Node B...")
        try:
            await self.node_b.connect()
            self.print_success("Node B connected")
        except Exception as e:
            self.print_error(f"Failed to connect Node B: {e}")
            if "Access denied" in str(e) or "PermissionError" in str(e) or "could not open port" in str(e).lower():
                self.print_info("Tip: Run 'python Scripts/check-port-availability.py' to diagnose port issues")
            raise
        
        await asyncio.sleep(1)  # Give nodes time to initialize
    
    async def cleanup(self):
        """Cleanup connections"""
        self.print_header("Cleaning Up")
        
        if self.node_a and self.node_a.is_connected():
            try:
                await self.node_a.stop_test()
            except:
                pass
            try:
                await self.node_a.disconnect()
                self.print_success("Node A disconnected")
            except Exception as e:
                self.print_error(f"Error disconnecting Node A: {e}")
        
        if self.node_b and self.node_b.is_connected():
            try:
                await self.node_b.stop_test()
            except:
                pass
            try:
                await self.node_b.disconnect()
                self.print_success("Node B disconnected")
            except Exception as e:
                self.print_error(f"Error disconnecting Node B: {e}")
    
    async def test_ping(self):
        """Test 1: PING command"""
        self.print_header("Test 1: PING (Connectivity Check)")
        
        await self.run_test("PING", "A", lambda: self.node_a.ping())
        await self.run_test("PING", "B", lambda: self.node_b.ping())
        
        await asyncio.sleep(0.5)
    
    async def test_node_type(self):
        """Test 2: NODE_TYPE command"""
        self.print_header("Test 2: NODE_TYPE (Get Node Type)")
        
        await self.run_test("NODE_TYPE", "A", lambda: self.node_a.get_firmware_node_type())
        await self.run_test("NODE_TYPE", "B", lambda: self.node_b.get_firmware_node_type())
        
        await asyncio.sleep(0.5)
    
    async def test_configure(self):
        """Test 3: SET_CONFIG command"""
        self.print_header("Test 3: SET_CONFIG (Configure UWB Parameters)")
        
        # Standard configuration
        config = {
            "channel": 5,
            "data_rate": "6m8",
            "preamble_len": 128,
            "payload_len": 64,
            "tx_power_idx": 5,
            "pkt_rate_hz": 100
        }
        
        self.print_info(f"Configuring with: {config}")
        
        await self.run_test("SET_CONFIG", "A", lambda: self.node_a.configure(config))
        await self.run_test("SET_CONFIG", "B", lambda: self.node_b.configure(config))
        
        await asyncio.sleep(1)
    
    async def test_get_stats(self):
        """Test 4: GET_STATS command (before test)"""
        self.print_header("Test 4: GET_STATS (Before Test)")
        
        await self.run_test("GET_STATS", "A", lambda: self.node_a.get_stats())
        await self.run_test("GET_STATS", "B", lambda: self.node_b.get_stats())
        
        await asyncio.sleep(0.5)
    
    async def test_reset_stats(self):
        """Test 5: RESET_STATS command"""
        self.print_header("Test 5: RESET_STATS (Reset Statistics)")
        
        await self.run_test("RESET_STATS", "A", lambda: self.node_a.reset_stats())
        await self.run_test("RESET_STATS", "B", lambda: self.node_b.reset_stats())
        
        await asyncio.sleep(0.5)
    
    async def test_start_test(self):
        """Test 6: START_TEST command"""
        self.print_header("Test 6: START_TEST (Start UWB Test)")
        
        await self.run_test("START_TEST", "A", lambda: self.node_a.start_test())
        await self.run_test("START_TEST", "B", lambda: self.node_b.start_test())
        
        await asyncio.sleep(2)  # Let test run for a bit
    
    async def test_get_stats_running(self):
        """Test 7: GET_STATS command (during test)"""
        self.print_header("Test 7: GET_STATS (During Test)")
        
        await self.run_test("GET_STATS (running)", "A", lambda: self.node_a.get_stats())
        await self.run_test("GET_STATS (running)", "B", lambda: self.node_b.get_stats())
        
        await asyncio.sleep(0.5)
    
    async def test_stop_test(self):
        """Test 8: STOP_TEST command"""
        self.print_header("Test 8: STOP_TEST (Stop UWB Test)")
        
        await self.run_test("STOP_TEST", "A", lambda: self.node_a.stop_test())
        await self.run_test("STOP_TEST", "B", lambda: self.node_b.stop_test())
        
        await asyncio.sleep(1)
    
    async def test_get_stats_after(self):
        """Test 9: GET_STATS command (after test)"""
        self.print_header("Test 9: GET_STATS (After Test)")
        
        await self.run_test("GET_STATS (after)", "A", lambda: self.node_a.get_stats())
        await self.run_test("GET_STATS (after)", "B", lambda: self.node_b.get_stats())
        
        await asyncio.sleep(0.5)
    
    async def test_set_log_mode(self):
        """Test 10: SET_LOG_MODE command"""
        self.print_header("Test 10: SET_LOG_MODE (Set Log Level)")
        
        # Test setting log mode to level 1
        await self.run_test("SET_LOG_MODE level=1", "A", 
                          lambda: self.node_a.rs485_comm.set_log_mode(1))
        await self.run_test("SET_LOG_MODE level=1", "B", 
                          lambda: self.node_b.rs485_comm.set_log_mode(1))
        
        await asyncio.sleep(0.5)
    
    async def test_multiple_configs(self):
        """Test 11: Multiple configuration scenarios"""
        self.print_header("Test 11: Multiple Configuration Scenarios")
        
        configs = [
            {"name": "Channel 9", "config": {"channel": 9, "data_rate": "6m8", "preamble_len": 128, "payload_len": 64, "tx_power_idx": 5, "pkt_rate_hz": 50}},
            {"name": "850k rate", "config": {"channel": 5, "data_rate": "850k", "preamble_len": 256, "payload_len": 32, "tx_power_idx": 3, "pkt_rate_hz": 10}},
            {"name": "Long preamble", "config": {"channel": 5, "data_rate": "6m8", "preamble_len": 512, "payload_len": 128, "tx_power_idx": 7, "pkt_rate_hz": 200}},
        ]
        
        for cfg_test in configs:
            self.print_info(f"Testing config: {cfg_test['name']}")
            await self.run_test(f"SET_CONFIG ({cfg_test['name']})", "A", 
                              lambda c=cfg_test['config']: self.node_a.configure(c))
            await self.run_test(f"SET_CONFIG ({cfg_test['name']})", "B", 
                              lambda c=cfg_test['config']: self.node_b.configure(c))
            await asyncio.sleep(0.5)
        
        # Restore default config
        default_config = {
            "channel": 5,
            "data_rate": "6m8",
            "preamble_len": 128,
            "payload_len": 64,
            "tx_power_idx": 5,
            "pkt_rate_hz": 100
        }
        await self.run_test("SET_CONFIG (restore default)", "A", 
                          lambda: self.node_a.configure(default_config))
        await self.run_test("SET_CONFIG (restore default)", "B", 
                          lambda: self.node_b.configure(default_config))
    
    def print_summary(self):
        """Print test summary"""
        self.print_header("Test Summary")
        
        total_tests = len(self.results)
        passed_tests = sum(1 for r in self.results if r.success)
        failed_tests = total_tests - passed_tests
        
        print(f"Total Tests: {total_tests}")
        print(f"{Colors.OKGREEN}Passed: {passed_tests}{Colors.ENDC}")
        print(f"{Colors.FAIL}Failed: {failed_tests}{Colors.ENDC}")
        print(f"Success Rate: {(passed_tests/total_tests*100):.1f}%")
        
        if failed_tests > 0:
            print(f"\n{Colors.FAIL}Failed Tests:{Colors.ENDC}")
            for result in self.results:
                if not result.success:
                    print(f"  {Colors.FAIL}✗{Colors.ENDC} {result.node_id}: {result.test_name}")
                    print(f"    Error: {result.error}")
        
        # Group by node
        node_a_results = [r for r in self.results if r.node_id == "A"]
        node_b_results = [r for r in self.results if r.node_id == "B"]
        
        print(f"\n{Colors.BOLD}Node A Results:{Colors.ENDC}")
        node_a_passed = sum(1 for r in node_a_results if r.success)
        print(f"  Passed: {node_a_passed}/{len(node_a_results)}")
        
        print(f"\n{Colors.BOLD}Node B Results:{Colors.ENDC}")
        node_b_passed = sum(1 for r in node_b_results if r.success)
        print(f"  Passed: {node_b_passed}/{len(node_b_results)}")
        
        # Average response times
        avg_time = sum(r.duration_ms for r in self.results) / len(self.results)
        print(f"\n{Colors.BOLD}Average Response Time:{Colors.ENDC} {avg_time:.1f}ms")
    
    async def run_all_tests(self):
        """Run all tests in sequence"""
        try:
            await self.setup()
            
            # Run all test suites
            await self.test_ping()
            await self.test_node_type()
            await self.test_configure()
            await self.test_get_stats()
            await self.test_reset_stats()
            await self.test_start_test()
            await self.test_get_stats_running()
            await self.test_stop_test()
            await self.test_get_stats_after()
            await self.test_set_log_mode()
            await self.test_multiple_configs()
            
            self.print_summary()
            
        except KeyboardInterrupt:
            self.print_error("Test interrupted by user")
        except Exception as e:
            self.print_error(f"Test suite error: {e}")
            import traceback
            traceback.print_exc()
        finally:
            await self.cleanup()


def load_env_config() -> Dict[str, Any]:
    """Load configuration from .env.win file"""
    env_file = Path(__file__).parent.parent / "Orchestrator" / ".env.win"
    config = {}
    
    if env_file.exists():
        with open(env_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, value = line.split('=', 1)
                    config[key.strip()] = value.strip()
    
    return config


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Comprehensive test script for both UWB nodes",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python test-all-commands.py
  python test-all-commands.py --node-a-port COM20 --node-b-port COM21
  python test-all-commands.py --baudrate 57600
        """
    )
    
    # Load default config from .env.win
    env_config = load_env_config()
    
    parser.add_argument(
        '--node-a-port',
        default=env_config.get('RS485_NODE_A_PORT', 'COM20'),
        help='Serial port for Node A (TX)'
    )
    parser.add_argument(
        '--node-b-port',
        default=env_config.get('RS485_NODE_B_PORT', 'COM21'),
        help='Serial port for Node B (RX)'
    )
    parser.add_argument(
        '--baudrate',
        type=int,
        default=int(env_config.get('BAUDRATE', 57600)),
        help='Serial baud rate'
    )
    
    args = parser.parse_args()
    
    print(f"{Colors.BOLD}{Colors.HEADER}")
    print("="*70)
    print("UWB Node Command Test Suite".center(70))
    print("="*70)
    print(f"{Colors.ENDC}")
    print(f"Node A (TX) Port: {args.node_a_port}")
    print(f"Node B (RX) Port: {args.node_b_port}")
    print(f"Baud Rate: {args.baudrate}")
    print(f"Start Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()
    
    tester = CommandTester(args.node_a_port, args.node_b_port, args.baudrate)
    
    try:
        asyncio.run(tester.run_all_tests())
    except KeyboardInterrupt:
        print(f"\n{Colors.WARNING}Test interrupted by user{Colors.ENDC}")
        sys.exit(1)
    except Exception as e:
        print(f"\n{Colors.FAIL}Fatal error: {e}{Colors.ENDC}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
