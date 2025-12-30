# UWB Transmission: Simple TX vs Orchestrator TX Comparison

## Key Insight: They Use the Same UWB API!

Both `simple_tx.c` and `orchestrator_tx_v2.c` use **identical UWB transmission code**:

```c
// Both examples use these same functions:
dwt_writetxdata(data_len, g_tx_buffer, 0);  // Write packet data
dwt_writetxfctrl(frame_len, 0, 0);          // Set frame control
dwt_starttx(DWT_START_TX_IMMEDIATE);        // Start transmission
waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0);  // Wait for completion
```

## Why Orchestrator TX Might Not Be Transmitting

### 1. **UWB Transmission Requires START Command**

**Critical Difference:**
- **Simple TX**: Starts transmitting immediately after boot
- **Orchestrator TX**: Only transmits after receiving `STRT` command via RS-485

**Check:**
```bash
# Send START command via RS-485
STRT
```

The orchestrator TX has a `g_test_running` flag that must be set to `1` before transmission begins.

### 2. **UWB Must Be Configured First**

**Orchestrator TX requires configuration before starting:**
```bash
# Configure UWB parameters
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100

# Then start transmission
STRT
```

**Simple TX**: Uses hardcoded configuration, no setup needed.

### 3. **Timer Must Be Initialized**

Orchestrator TX uses a timer (`app_timer`) to send packets periodically. If timer initialization fails, transmission won't start even with `STRT` command.

**Check logs for:**
```
TIMER INIT FAILED
```

### 4. **Configuration Mismatch Between TX and RX**

Both nodes must use **identical** UWB parameters:
- Channel (must match)
- Data rate (must match)
- Preamble length (must match)
- Preamble code (must match)

## Side-by-Side Comparison

| Feature | Simple TX | Orchestrator TX |
|---------|-----------|-----------------|
| **UWB TX API** | ✅ Same | ✅ Same |
| **Starts Immediately** | ✅ Yes | ❌ No (needs START) |
| **RS-485 Control** | ❌ No | ✅ Yes |
| **Configuration** | Hardcoded | Via RS-485 commands |
| **Timer-based** | ❌ No (fixed delay) | ✅ Yes (configurable rate) |
| **Statistics** | ❌ No | ✅ Yes |
| **Error Handling** | Basic | Enhanced |

## Diagnostic Steps

### Step 1: Verify RS-485 Communication Works

```bash
# Send ping command
PNG

# Expected response:
OK
```

If this works, RS-485 is functioning correctly.

### Step 2: Check Node Type

```bash
# Check which node type is running
NODE_TYPE

# Expected responses:
OK NODE_TYPE=TX_V2    # For TX node
OK NODE_TYPE=RX_V2    # For RX node
```

### Step 3: Configure UWB Parameters

```bash
# Configure TX node
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100

# Expected response:
OK CONFIG
```

### Step 4: Start Transmission

```bash
# Start TX on Node A
STRT

# Expected response:
OK START
```

### Step 5: Check Statistics

```bash
# Check TX statistics
STAT

# Expected response (TX node):
OK STATS sent=<count> attempted=<count> errors=<count> timeouts=<count> ...

# Expected response (RX node):
OK STATS rx=<count> lost=<count> crc_err=<count> ...
```

## Common Issues and Solutions

### Issue 1: TX Node Not Transmitting

**Symptoms:**
- RS-485 commands work (LEDs blink)
- `STAT` shows `sent=0 attempted=0`

**Causes:**
1. **Not started**: `STRT` command not sent
2. **Not configured**: `CFG` command not sent
3. **Timer not initialized**: Check logs for timer errors
4. **DW3000 not initialized**: Check logs for initialization errors

**Solution:**
```bash
# Ensure proper sequence:
1. PNG          # Verify RS-485 works
2. CFG ...      # Configure UWB
3. STRT         # Start transmission
4. STAT         # Check if packets are being sent
```

### Issue 2: RX Node Not Receiving

**Symptoms:**
- TX node shows `sent>0` in statistics
- RX node shows `rx=0`

**Causes:**
1. **Configuration mismatch**: Different channel/data rate/preamble
2. **RX not started**: RX node needs `STRT` command too
3. **Physical distance**: Too far apart
4. **Antenna issues**: Antennas not connected properly

**Solution:**
```bash
# On RX node:
1. CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100  # Match TX config
2. STRT                                        # Start listening
3. STAT                                        # Check reception
```

### Issue 3: Both Nodes Work But No Communication

**Symptoms:**
- TX: `sent>0`
- RX: `rx=0`
- Both RS-485 working

**Causes:**
1. **Different channels**: Most common issue
2. **Different preamble codes**: Must match
3. **Different data rates**: Must match
4. **Hardware issue**: Antenna, UWB chip, or wiring

**Solution:**
```bash
# Verify both nodes have identical configuration:
# On TX node:
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
STAT

# On RX node:
CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100
STAT

# Both should show same channel, rate, preamble length
```

## Why Simple TX Works But Orchestrator TX Doesn't

If `simple_tx.c` works but `orchestrator_tx_v2.c` doesn't:

### 1. **Orchestrator Requires Commands**
Simple TX starts immediately. Orchestrator TX needs:
- `CFG` command to configure
- `STRT` command to start

### 2. **Timer Dependency**
Orchestrator TX uses `app_timer` which might fail initialization. Simple TX uses simple `Sleep()` delay.

### 3. **State Management**
Orchestrator TX has state flags (`g_test_running`, `g_config.configured`) that must be set correctly.

### 4. **RS-485 Interference**
If RS-485 is interfering with UWB (unlikely but possible), simple TX won't have this issue.

## Quick Test: Use Simple TX Logic in Orchestrator

If you want orchestrator TX to work like simple TX (start immediately), you can modify `orchestrator_tx_v2.c`:

```c
// In orchestrator_tx_v2.c, modify the main loop:
int orchestrator_tx_v2(void)
{
    // ... initialization code ...
    
    // Add this after configure_uwb():
    g_config.configured = 1;  // Auto-configure
    g_test_running = 1;        // Auto-start
    
    // Start timer immediately
    uint32_t period_ms = 1000 / g_config.pkt_rate_hz;
    app_timer_start(m_tx_timer_id, APP_TIMER_TICKS(period_ms), NULL);
    
    // ... rest of code ...
}
```

## Conclusion

**If RS-485 works (LEDs blink), UWB CAN work!** The orchestrator TX uses the same UWB transmission code as simple TX. The difference is:

1. **Orchestrator TX requires commands** to configure and start
2. **Simple TX starts automatically** with hardcoded settings
3. **Both use identical UWB APIs** - the transmission mechanism is the same

**To make orchestrator TX work:**
1. Ensure RS-485 communication works (✅ you have this)
2. Send `CFG` command to configure UWB
3. Send `STRT` command to start transmission
4. Verify with `STAT` command

The UWB hardware is the same, the API is the same - orchestrator just adds RS-485 control and state management on top!

