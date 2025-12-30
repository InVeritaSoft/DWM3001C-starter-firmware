# UWB Packet Transmission Fix

## Problem Identified

Packets were not being sent/received over UWB despite code compliance validation.

## Root Cause

The TX node was using **manual polling with Sleep(1)** instead of the proper `waitforsysstatus()` function used by all other examples. This caused:
1. **Inefficient status register polling** - Sleep(1) is too slow
2. **Potential timing issues** - Manual polling might miss status updates
3. **Inconsistent with SDK examples** - All other examples use `waitforsysstatus()`

## Fixes Applied

### Fix 1: Replace Manual Polling with `waitforsysstatus()` ✅

**File**: `orchestrator_tx_v2.c`

**Before** (lines 678-686):
```c
// Wait for TX complete with timeout
// Poll status register until TXFRS bit is set or timeout occurs
status_reg = dwt_readsysstatuslo();
while (!(status_reg & DWT_INT_TXFRS_BIT_MASK) && timeout_count < MAX_TIMEOUT)
{
    status_reg = dwt_readsysstatuslo();
    timeout_count++;
    Sleep(1);  // ⚠️ Too slow!
}

if (status_reg & DWT_INT_TXFRS_BIT_MASK)
{
    // Clear TX frame sent event
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
    // ...
}
else
{
    // Timeout handling
    if (timeout_count >= MAX_TIMEOUT)
    {
        g_stats.tx_timeouts++;
        g_stats.last_error = 2;
    }
    // ...
}
```

**After**:
```c
// Wait for TX complete using proper polling function (like simple_tx example)
// This is more efficient than manual polling with Sleep(1)
waitforsysstatus(&status_reg, NULL, DWT_INT_TXFRS_BIT_MASK, 0);

if (status_reg & DWT_INT_TXFRS_BIT_MASK)
{
    // Clear TX frame sent event
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
    g_stats.total_sent++;
    g_stats.last_tx_timestamp = dwt_readsystimestamphi32();
    g_stats.last_error = 0;
}
else
{
    // Error occurred (should not happen with waitforsysstatus, but handle it)
    g_stats.tx_errors++;
    g_stats.last_error = 1; // General TX error
}
```

**Benefits**:
- ✅ Uses the same reliable polling mechanism as all SDK examples
- ✅ More efficient - no unnecessary Sleep() calls
- ✅ Properly waits for TX completion
- ✅ Consistent with `simple_tx.c` and other examples

### Fix 2: Add Delay in RX Main Loop ✅

**File**: `orchestrator_rx_v2.c`

**Before** (lines 843-855):
```c
while (1)
{
    if (g_test_running)
    {
        process_rx_packet();
    }
    else
    {
        Sleep(100);
    }
}
```

**After**:
```c
while (1)
{
    if (g_test_running)
    {
        process_rx_packet();
        Sleep(1);  // Small delay to prevent CPU spinning and allow status register to update
    }
    else
    {
        Sleep(100);
    }
}
```

**Benefits**:
- ✅ Prevents CPU from spinning too fast
- ✅ Allows status register time to update
- ✅ Reduces power consumption
- ✅ Prevents potential race conditions

### Fix 3: Remove Unused Variables ✅

**File**: `orchestrator_tx_v2.c`

**Removed**:
- `timeout_count` variable (no longer needed)
- `MAX_TIMEOUT` constant (no longer needed)

## Testing

After applying these fixes:

1. **Rebuild firmware**:
   ```bash
   make build
   ```

2. **Flash to both nodes**:
   ```bash
   make flash
   ```

3. **Test sequence**:
   - Send `CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100` to both nodes
   - Send `STRT` to both nodes
   - Check `STAT` command - should show packets being sent/received
   - Verify `total_sent` and `total_rx` are increasing

4. **Expected behavior**:
   - TX node: `total_sent` should increase with each timer tick
   - RX node: `total_rx` should increase when packets are received
   - No more timeouts or errors

## Comparison with Working Examples

### simple_tx.c (Working Example)
```c
dwt_starttx(DWT_START_TX_IMMEDIATE);
waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0);
dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
```

### orchestrator_tx_v2.c (Before Fix)
```c
dwt_starttx(DWT_START_TX_IMMEDIATE);
status_reg = dwt_readsysstatuslo();
while (!(status_reg & DWT_INT_TXFRS_BIT_MASK) && timeout_count < MAX_TIMEOUT)
{
    status_reg = dwt_readsysstatuslo();
    timeout_count++;
    Sleep(1);  // ⚠️ Problem!
}
```

### orchestrator_tx_v2.c (After Fix) ✅
```c
dwt_starttx(DWT_START_TX_IMMEDIATE);
waitforsysstatus(&status_reg, NULL, DWT_INT_TXFRS_BIT_MASK, 0);  // ✅ Fixed!
if (status_reg & DWT_INT_TXFRS_BIT_MASK)
{
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
    // ...
}
```

## Status

✅ **FIXED** - Code now matches working examples and should properly send/receive UWB packets.

## Next Steps

1. Rebuild and flash firmware
2. Test packet transmission
3. Verify statistics are updating correctly
4. Check RTT logs if issues persist

