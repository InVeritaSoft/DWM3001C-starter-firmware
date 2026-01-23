# Test All Commands Script

Comprehensive test scripts to verify all available commands on both UWB nodes (Node A/TX and Node B/RX).

## Available Scripts

### Python Script (Recommended)
- **File**: `test-all-commands.py`
- **Platform**: Cross-platform (Windows, Linux, macOS)
- **Requirements**: Python 3.7+, orchestrator dependencies

### PowerShell Script
- **File**: `test-all-commands.ps1`
- **Platform**: Windows only
- **Requirements**: PowerShell 5.1+

## Usage

### Python Script

```bash
# Basic usage (uses ports from .env.win)
python Scripts/test-all-commands.py

# Specify custom ports
python Scripts/test-all-commands.py --node-a-port COM20 --node-b-port COM21

# Specify custom baud rate
python Scripts/test-all-commands.py --baudrate 57600

# All options
python Scripts/test-all-commands.py --node-a-port COM20 --node-b-port COM21 --baudrate 57600
```

### PowerShell Script

```powershell
# Basic usage (uses ports from .env.win)
.\Scripts\test-all-commands.ps1

# Specify custom ports
.\Scripts\test-all-commands.ps1 -NodeAPort COM20 -NodeBPort COM21

# Specify custom baud rate
.\Scripts\test-all-commands.ps1 -Baudrate 57600

# All options
.\Scripts\test-all-commands.ps1 -NodeAPort COM20 -NodeBPort COM21 -Baudrate 57600
```

## Configuration

The scripts automatically load configuration from `Orchestrator/.env.win`:
- `RS485_NODE_A_PORT` - Port for Node A (TX)
- `RS485_NODE_B_PORT` - Port for Node B (RX)
- `BAUDRATE` - Serial communication baud rate

Command-line arguments override these defaults.

## Test Coverage

The script tests all available commands on both nodes:

1. **PING** - Connectivity check
2. **NODE_TYPE** - Get node type (TX_V2 or RX_V2)
3. **SET_CONFIG** - Configure UWB parameters
4. **GET_STATS** - Get statistics (before test)
5. **RESET_STATS** - Reset statistics
6. **START_TEST** - Start UWB test
7. **GET_STATS** - Get statistics (during test)
8. **STOP_TEST** - Stop UWB test
9. **GET_STATS** - Get statistics (after test)
10. **SET_LOG_MODE** - Set log level
11. **Multiple Configurations** - Test various configuration scenarios

## Expected Output

The script provides:
- Color-coded output (green for success, red for errors)
- Detailed test results for each command
- Summary statistics:
  - Total tests run
  - Pass/fail counts
  - Success rate
  - Per-node statistics
  - Average response times

## Example Output

```
======================================================================
                    UWB Node Command Test Suite
======================================================================

Node A (TX) Port: COM20
Node B (RX) Port: COM21
Baud Rate: 57600
Start Time: 2026-01-23 14:30:00

======================================================================
                        Setting Up Connections
======================================================================

[INFO] Connecting to Node A...
[✓] Node A connected
[INFO] Connecting to Node B...
[✓] Node B connected

======================================================================
                    Test 1: PING (Connectivity Check)
======================================================================

[TEST] A: PING
[✓] A: PING - OK
[TEST] B: PING
[✓] B: PING - OK

...

======================================================================
                            Test Summary
======================================================================

Total Tests: 22
Passed: 22
Failed: 0
Success Rate: 100.0%

Node A Results:
  Passed: 11/11

Node B Results:
  Passed: 11/11

Average Response Time: 245.3ms
```

## Troubleshooting

### Port Not Found
- Verify the COM ports are correct
- Check Device Manager (Windows) or `ls /dev/tty*` (Linux/Mac)
- Ensure no other application is using the ports

### Connection Timeout
- Verify RS-485 hardware is connected
- Check baud rate matches firmware (default: 57600)
- Ensure firmware is running orchestrator example
- Check LED indicators on boards

### Command Failures
- Verify firmware is running the correct version (orchestrator v2)
- Check that nodes are properly configured
- Review error messages for specific issues

### Python Dependencies
If running the Python script, ensure dependencies are installed:
```bash
cd Orchestrator/python_backend
pip install -r requirements.txt
```

## Notes

- The script automatically stops any running tests during cleanup
- Tests are run sequentially to avoid conflicts
- Response times are logged for performance monitoring
- All test results are stored in memory for summary reporting
