# Troubleshooting: RS-485 Works But UWB Transmission Doesn't Start

## Problem Description

**Symptom:** 
- ✅ RS-485 commands are received (LED blinks when commands sent)
- ✅ Commands are processed (responses received)
- ❌ UWB packet exchange doesn't start between Node A and Node B

## Root Cause Analysis

### Why Commands Are Received But UWB Doesn't Start

The orchestrator firmware has **multiple layers** that must all be working:

1. **RS-485 Layer** ✅ (Working - LED blinks)
2. **Command Parsing Layer** ✅ (Working - responses received)
3. **UWB Configuration Layer** ❓ (May not be configured)
4. **Timer Initialization Layer** ❓ (May have failed)
5. **UWB Transmission Layer** ❓ (May not be starting)

## Diagnostic Checklist

### Step 1: Verify Both Nodes Are Configured

**TX Node (Node A):**
```bash
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
```
Expected: `OK CONFIG`

**RX Node (Node B):**
```bash
CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100
```
Expected: `OK CONFIG`

**Critical:** Both nodes MUST have:
- ✅ Same channel (ch=5)
- ✅ Same data rate (rate=6m8)
- ✅ Same preamble length (pl=128)
- ✅ Same preamble code (default: 9)

### Step 2: Verify Both Nodes Are Started

**TX Node:**
```bash
STRT
```
Expected: `OK START`

**RX Node:**
```bash
STRT
```
Expected: `OK START`

**Critical:** BOTH nodes need START command!

### Step 3: Check for Error Responses

When you send `STRT`, check the response:

| Response | Meaning | Solution |
|----------|---------|----------|
| `OK START` | ✅ Success | Proceed to Step 4 |
| `ERR NOT_CONFIGURED` | ❌ UWB not configured | Send `CFG` command first |
| `ERR TIMER_NOT_INIT` | ❌ Timer failed to initialize | Check RTT logs, firmware issue |
| `ERR START_FAILED` | ❌ Timer start failed | Check RTT logs |

### Step 4: Check Statistics

Wait 3-5 seconds after START, then check:

**TX Node:**
```bash
STAT
```
Look for:
- `sent=X` - Should be > 0 if transmitting
- `attempted=X` - Should be > 0 if timer is firing
- `errors=X` - Should be 0 if working correctly
- `timeouts=X` - Should be 0 if DW3000 is responding

**RX Node:**
```bash
STAT
```
Look for:
- `rx=X` - Should be > 0 if receiving packets
- `crc_err=X` - CRC errors (packets received but corrupted)
- `lost=X` - Lost packets (sequence gaps)

## Common Issues and Solutions

### Issue 1: "ERR NOT_CONFIGURED" When Sending START

**Cause:** UWB parameters not configured

**Solution:**
```bash
# On TX node:
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100

# On RX node:
CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100

# Then start both:
STRT
```

### Issue 2: "ERR TIMER_NOT_INIT" When Sending START

**Cause:** Timer module failed to initialize during boot

**Symptoms:**
- RS-485 works
- Commands are received
- But timer initialization failed

**Check RTT logs for:**
```
TIMER INIT FAILED
```

**Possible Causes:**
- Insufficient memory
- Timer module conflict
- Hardware issue

**Solution:**
1. Check RTT debug output during boot
2. Look for timer initialization errors
3. May need to reduce memory usage or fix timer initialization

### Issue 3: START Succeeds But No Packets Sent (sent=0)

**Symptoms:**
- `STRT` returns `OK START`
- But `STAT` shows `sent=0 attempted=0`

**Possible Causes:**

#### A. Timer Not Firing
- Timer started but handler not being called
- Check RTT logs for timer-related messages
- Verify `tx_timer_handler()` is being called

#### B. g_test_running Flag Not Set
- Timer fires but `g_test_running` is still 0
- Check if START command actually set the flag
- May be a race condition

#### C. send_packet() Failing Silently
- Timer fires, flag is set, but `send_packet()` fails
- Check for DW3000 errors
- Verify DW3000 is in correct state

**Diagnostic:**
```bash
# Check if timer is firing by looking at attempted count
STAT
# If attempted > 0 but sent = 0, send_packet() is failing
# If attempted = 0, timer is not firing
```

### Issue 4: TX Sends But RX Doesn't Receive

**Symptoms:**
- TX: `sent > 0`
- RX: `rx = 0`

**Possible Causes:**

#### A. Configuration Mismatch
**Most Common!**

Check both nodes have identical:
- Channel (must match exactly)
- Data rate (must match exactly)
- Preamble length (must match exactly)
- Preamble code (must match exactly)

**Solution:**
```bash
# On TX node:
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
STAT  # Note the configuration

# On RX node:
CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100  # Must match TX!
STAT  # Verify configuration matches
```

#### B. RX Not Started
RX node needs `STRT` command to enable reception!

**Solution:**
```bash
# On RX node:
STRT
```

#### C. Physical Issues
- Too far apart
- Obstacles blocking signal
- Antennas not connected
- Wrong antenna orientation

**Solution:**
- Place nodes close together (< 1 meter) for testing
- Verify antennas are connected
- Check for obstacles

### Issue 5: Both Nodes Started But No Communication

**Checklist:**
1. ✅ Both nodes configured with same parameters?
2. ✅ Both nodes started with `STRT`?
3. ✅ TX node showing `sent > 0`?
4. ✅ RX node showing `rx > 0`?
5. ✅ Physical distance reasonable?
6. ✅ Antennas connected?

If all above are yes but still no communication:
- Check RTT logs for DW3000 initialization errors
- Verify DW3000 hardware is working (try simple_tx/simple_rx examples)
- Check for hardware conflicts

## Using the Diagnostic Script

Run the diagnostic script on both nodes:

```powershell
# On TX node (Node A):
.\diagnose-uwb-issue.ps1 COM3

# On RX node (Node B):
.\diagnose-uwb-issue.ps1 COM4
```

The script will:
1. Verify RS-485 communication
2. Check node type
3. Verify configuration
4. Start transmission/reception
5. Check statistics
6. Provide specific diagnostics

## Quick Test: Compare with Simple Examples

To verify UWB hardware is working, test with simple examples:

**TX Node:**
```powershell
.\set-simple-tx.ps1
make build
make flash
```

**RX Node:**
```powershell
.\set-simple-rx.ps1
make build
make flash
```

If simple examples work but orchestrator doesn't:
- The issue is in orchestrator firmware logic
- Check timer initialization
- Check state flags
- Check command processing

## Expected Behavior

### When Everything Works:

**TX Node:**
```
PNG → OK
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100 → OK CONFIG
STRT → OK START
STAT → OK STATS sent=30 attempted=30 errors=0 timeouts=0 ...
```

**RX Node:**
```
PNG → OK
CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100 → OK CONFIG
STRT → OK START
STAT → OK STATS rx=30 lost=0 crc_err=0 ...
```

## Debugging Tips

1. **Check RTT Logs:** Use RTT viewer to see debug messages during boot and operation
2. **Use STAT Command:** Check statistics frequently to see what's happening
3. **Test One Node at a Time:** Verify TX works first, then RX
4. **Start Simple:** Use simple_tx/simple_rx to verify hardware works
5. **Check Responses:** Always check command responses for error messages

## Summary

**If RS-485 works (LED blinks) but UWB doesn't start:**

1. ✅ RS-485 communication: Working
2. ✅ Command parsing: Working
3. ❓ UWB configuration: Check with `CFG` command
4. ❓ Timer initialization: Check RTT logs
5. ❓ Both nodes started: Both need `STRT`
6. ❓ Configuration match: Both nodes must have identical UWB settings

The most common issue is **configuration mismatch** or **forgetting to send START command on both nodes**.

