# Manual UWB Test Procedure

Since RS-485 is working (LEDs blink), let's manually test UWB step by step.

## Prerequisites

- Two boards: Node A (TX) and Node B (RX)
- Serial terminal or PowerShell script
- Both boards powered on

## Step-by-Step Test

### Step 1: Test Node A (TX)

Open serial terminal to Node A's COM port (e.g., COM11):

```bash
# Test 1: Ping
PNG
# Expected: OK

# Test 2: Check node type
NODE_TYPE
# Expected: OK NODE_TYPE=TX_V2

# Test 3: Configure UWB
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
# Expected: OK CONFIG

# Test 4: Start transmission
STRT
# Expected: OK START

# Test 5: Wait 3 seconds, then check statistics
STAT
# Expected: OK STATS sent=X attempted=X ...
# IMPORTANT: Check if sent > 0 and attempted > 0
```

**What to look for:**
- ✅ If `sent > 0`: TX is working! Packets are being sent.
- ❌ If `sent = 0` but `attempted > 0`: Timer is firing but packets failing to send.
- ❌ If `attempted = 0`: Timer is NOT firing (check for "ERR TIMER_NOT_INIT" or timer issues).

### Step 2: Test Node B (RX)

Open serial terminal to Node B's COM port (e.g., COM12):

```bash
# Test 1: Ping
PNG
# Expected: OK

# Test 2: Check node type
NODE_TYPE
# Expected: OK NODE_TYPE=RX_V2

# Test 3: Configure UWB (MUST MATCH TX!)
CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100
# Expected: OK CONFIG
# CRITICAL: Channel, rate, and preamble MUST match TX!

# Test 4: Start reception
STRT
# Expected: OK START

# Test 5: Wait 3 seconds, then check statistics
STAT
# Expected: OK STATS rx=X lost=X ...
# IMPORTANT: Check if rx > 0
```

**What to look for:**
- ✅ If `rx > 0`: RX is working! Packets are being received.
- ❌ If `rx = 0`: No packets received (check configuration match, distance, antennas).

### Step 3: Verify Configuration Match

**CRITICAL:** Both nodes MUST have identical:
- Channel (ch=5)
- Data rate (rate=6m8)
- Preamble length (pl=128)
- Preamble code (default: 9)

**Check on both nodes:**
```bash
STAT
# Look at the response - it should show configuration
# Or check what you sent in CFG command
```

## Common Issues

### Issue A: TX shows `attempted=0`

**Meaning:** Timer is not firing.

**Possible causes:**
1. Timer initialization failed during boot
2. `g_timer_initialized` flag is false
3. START command returned "ERR TIMER_NOT_INIT"

**Check:**
- Look at START command response - did it say "OK START" or "ERR TIMER_NOT_INIT"?
- Check RTT logs for timer initialization errors

**Solution:**
- If "ERR TIMER_NOT_INIT": Timer module failed to initialize - firmware issue
- If "OK START" but `attempted=0`: Timer started but handler not being called - check RTT logs

### Issue B: TX shows `attempted > 0` but `sent = 0`

**Meaning:** Timer is firing but packets aren't being sent successfully.

**Possible causes:**
1. DW3000 not responding to TX commands
2. DW3000 hardware issue
3. Transmission errors/timeouts

**Check:**
```bash
STAT
# Look for:
# - errors=X (should be 0)
# - timeouts=X (should be 0)
```

**Solution:**
- If `timeouts > 0`: DW3000 not responding - check hardware, SPI connection
- If `errors > 0`: Transmission errors - check DW3000 state

### Issue C: TX shows `sent > 0` but RX shows `rx = 0`

**Meaning:** TX is working but RX isn't receiving.

**Possible causes:**
1. Configuration mismatch (most common!)
2. RX not started
3. Physical distance/obstacles
4. Antenna issues

**Check:**
1. Verify both nodes have identical configuration:
   ```bash
   # On TX:
   CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
   
   # On RX:
   CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100
   ```
   **MUST MATCH:** Channel, rate, preamble length

2. Verify RX is started:
   ```bash
   # On RX:
   STRT
   # Should return: OK START
   ```

3. Check physical setup:
   - Place nodes close together (< 1 meter) for testing
   - Verify antennas are connected
   - Check for obstacles

## Using the Interactive Tool

I've created a better tool for manual testing:

```powershell
# On Node A (TX):
.\check-uwb-status.ps1 COM11

# On Node B (RX):
.\check-uwb-status.ps1 COM12
```

This tool:
- Shows current status
- Lets you send commands interactively
- Better response parsing
- Shows statistics clearly

## Quick Diagnostic Checklist

Run through this checklist:

### Node A (TX):
- [ ] PNG returns OK
- [ ] NODE_TYPE returns TX_V2
- [ ] CFG returns OK CONFIG
- [ ] STRT returns OK START (NOT "ERR TIMER_NOT_INIT")
- [ ] STAT shows `attempted > 0` (timer firing)
- [ ] STAT shows `sent > 0` (packets being sent)

### Node B (RX):
- [ ] PNG returns OK
- [ ] NODE_TYPE returns RX_V2
- [ ] CFG returns OK CONFIG (with matching parameters)
- [ ] STRT returns OK START
- [ ] STAT shows `rx > 0` (packets being received)

### Configuration Match:
- [ ] Both nodes use same channel (ch=5)
- [ ] Both nodes use same data rate (rate=6m8)
- [ ] Both nodes use same preamble length (pl=128)

## Expected Working Output

### Node A (TX):
```
PNG → OK
NODE_TYPE → OK NODE_TYPE=TX_V2
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100 → OK CONFIG
STRT → OK START
STAT → OK STATS sent=30 attempted=30 errors=0 timeouts=0 ...
```

### Node B (RX):
```
PNG → OK
NODE_TYPE → OK NODE_TYPE=RX_V2
CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100 → OK CONFIG
STRT → OK START
STAT → OK STATS rx=30 lost=0 crc_err=0 ...
```

## Next Steps Based on Results

**If TX shows `attempted=0`:**
- Check START response - did it say "ERR TIMER_NOT_INIT"?
- Check RTT logs for timer initialization errors
- This is a firmware/timer issue

**If TX shows `sent=0` but `attempted > 0`:**
- Check STAT for errors/timeouts
- Check DW3000 hardware connection
- Check SPI communication

**If TX shows `sent > 0` but RX shows `rx=0`:**
- Verify configuration match (most common issue!)
- Verify RX is started
- Check physical setup (distance, antennas)

**If both show packets:**
- ✅ Everything is working!
- If packet counts don't match, that's normal (some packets may be lost)

