# UWB Packet Transmission Diagnosis

## Problem
Packets are not being sent/received over UWB despite code compliance validation.

## Potential Issues Found

### Issue 1: TX Timer May Not Be Firing ⚠️

**Location**: `orchestrator_tx_v2.c` line 634-640

```c
static void tx_timer_handler(void *p_context)
{
    if (g_test_running)
    {
        send_packet();
    }
}
```

**Problem**: Timer handler only sends if `g_test_running` is true, but timer might not be started properly.

**Check**: When `STRT` command is received, verify:
1. Timer is created successfully (`g_timer_initialized == 1`)
2. Timer is started successfully (`app_timer_start()` returns `NRF_SUCCESS`)
3. Timer period is calculated correctly

**Current Code** (line 561-565):
```c
uint32_t period_ms = 1000 / g_config.pkt_rate_hz;
if (period_ms < 1) period_ms = 1;
uint32_t err_code = app_timer_start(m_tx_timer_id, APP_TIMER_TICKS(period_ms), NULL);
```

**Potential Issue**: If `pkt_rate_hz` is 0 or very large, period_ms calculation might be wrong.

### Issue 2: TX Status Polling May Be Too Slow ⚠️

**Location**: `orchestrator_tx_v2.c` line 680-686

```c
status_reg = dwt_readsysstatuslo();
while (!(status_reg & DWT_INT_TXFRS_BIT_MASK) && timeout_count < MAX_TIMEOUT)
{
    status_reg = dwt_readsysstatuslo();
    timeout_count++;
    Sleep(1);  // ⚠️ Sleep(1) might be too slow!
}
```

**Problem**: `Sleep(1)` might be too slow for proper status polling. The simple_tx example uses `waitforsysstatus()` which is more efficient.

**Comparison with simple_tx**:
- `simple_tx.c` uses: `waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0);`
- `orchestrator_tx_v2.c` uses: Manual polling with `Sleep(1)`

**Recommendation**: Use `waitforsysstatus()` instead of manual polling.

### Issue 3: RX Status Polling May Not Be Frequent Enough ⚠️

**Location**: `orchestrator_rx_v2.c` line 843-855

```c
while (1)
{
    if (g_test_running)
    {
        process_rx_packet();
    }
    else
    {
        Sleep(100);  // ⚠️ Sleeps for 100ms when test not running
    }
}
```

**Problem**: When test is running, `process_rx_packet()` is called continuously, but there's no delay between calls. This might cause:
1. CPU spinning too fast
2. Status register not having time to update
3. Missing packets if status register is checked too quickly

**Recommendation**: Add a small delay (1-10ms) even when test is running.

### Issue 4: RX Not Enabled Before START Command ⚠️

**Location**: `orchestrator_rx_v2.c` line 517-535

```c
else if (strcmp(cmd_upper, "STRT") == 0 || strcmp(cmd_upper, "START_TEST") == 0)
{
    // ...
    // Ensure RX is enabled
    dwt_rxenable(DWT_START_RX_IMMEDIATE);
    
    send_response("OK START");
}
```

**Problem**: RX is only enabled when START command is received. If TX node starts sending immediately, RX might miss the first packets.

**Recommendation**: Enable RX earlier, or add a small delay after enabling RX.

### Issue 5: Configuration Not Applied Correctly ⚠️

**Location**: `orchestrator_tx_v2.c` line 616-629

```c
static void configure_uwb(void)
{
    if (dwt_configure(&dwt_config))
    {
        test_run_info((unsigned char *)"CONFIG FAILED");
        return;  // ⚠️ Returns without error indication!
    }
    // ...
}
```

**Problem**: If configuration fails, function returns silently. No error is sent to orchestrator.

**Recommendation**: Send error response if configuration fails.

### Issue 6: TX Power Configuration May Be Wrong ⚠️

**Location**: `orchestrator_tx_v2.c` line 625

```c
// Configure TX power with boost calculation
configure_tx_power();
```

**Problem**: TX power configuration happens in `configure_uwb()`, but if TX power configuration fails, no error is reported.

**Check**: Verify TX power is actually configured correctly by checking:
1. `configure_tx_power()` function implementation
2. Whether `dwt_adjust_tx_power()` is called correctly
3. Whether power boost calculation is correct

## Diagnostic Steps

### Step 1: Verify Timer is Working

**Add debug output**:
```c
static void tx_timer_handler(void *p_context)
{
    test_run_info((unsigned char *)"TIMER FIRED");
    if (g_test_running)
    {
        test_run_info((unsigned char *)"SENDING PACKET");
        send_packet();
    }
    else
    {
        test_run_info((unsigned char *)"TEST NOT RUNNING");
    }
}
```

**Check RTT logs** to see if timer is firing.

### Step 2: Verify Packet Transmission

**Add debug output in `send_packet()`**:
```c
static void send_packet(void)
{
    test_run_info((unsigned char *)"SEND_PACKET CALLED");
    // ... rest of function
}
```

**Check RTT logs** to see if `send_packet()` is being called.

### Step 3: Verify Status Register

**Add debug output**:
```c
status_reg = dwt_readsysstatuslo();
test_run_info((unsigned char *)"STATUS REG CHECK");
while (!(status_reg & DWT_INT_TXFRS_BIT_MASK) && timeout_count < MAX_TIMEOUT)
{
    status_reg = dwt_readsysstatuslo();
    timeout_count++;
    Sleep(1);
}

if (status_reg & DWT_INT_TXFRS_BIT_MASK)
{
    test_run_info((unsigned char *)"TX SUCCESS");
}
else
{
    test_run_info((unsigned char *)"TX TIMEOUT");
}
```

### Step 4: Verify RX is Receiving

**Add debug output in RX node**:
```c
static void process_rx_packet(void)
{
    uint32_t status_reg = dwt_readsysstatuslo();
    
    if (status_reg & DWT_INT_RXFCG_BIT_MASK)
    {
        test_run_info((unsigned char *)"RX PACKET RECEIVED");
        // ... process packet
    }
    else if (status_reg & SYS_STATUS_ALL_RX_ERR)
    {
        test_run_info((unsigned char *)"RX ERROR");
        // ... handle error
    }
}
```

## Recommended Fixes

### Fix 1: Use `waitforsysstatus()` for TX

**Replace** (line 680-686):
```c
status_reg = dwt_readsysstatuslo();
while (!(status_reg & DWT_INT_TXFRS_BIT_MASK) && timeout_count < MAX_TIMEOUT)
{
    status_reg = dwt_readsysstatuslo();
    timeout_count++;
    Sleep(1);
}
```

**With**:
```c
if (waitforsysstatus(NULL, NULL, DWT_INT_TXFRS_BIT_MASK, 0) != DWT_SUCCESS)
{
    // Timeout or error
    g_stats.tx_timeouts++;
    g_stats.last_error = 2;
    return;
}
```

### Fix 2: Add Delay in RX Main Loop

**Replace** (line 843-855):
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

**With**:
```c
while (1)
{
    if (g_test_running)
    {
        process_rx_packet();
        Sleep(1);  // Small delay to prevent CPU spinning
    }
    else
    {
        Sleep(100);
    }
}
```

### Fix 3: Add Error Handling for Configuration

**Add** in `configure_uwb()`:
```c
static void configure_uwb(void)
{
    if (dwt_configure(&dwt_config))
    {
        test_run_info((unsigned char *)"CONFIG FAILED");
        send_response("ERR UWB_CONFIG_FAILED");
        return;
    }
    // ...
}
```

### Fix 4: Verify Timer Period Calculation

**Add validation**:
```c
uint32_t period_ms = 1000 / g_config.pkt_rate_hz;
if (period_ms < 1) period_ms = 1;
if (period_ms > 1000) period_ms = 1000;  // Max 1 second

// Debug output
char debug_msg[64];
snprintf(debug_msg, sizeof(debug_msg), "TIMER PERIOD: %lu ms", period_ms);
test_run_info((unsigned char *)debug_msg);
```

## Quick Test

1. **Send CFG command**: Verify configuration is applied
2. **Send STRT command**: Verify timer starts
3. **Check RTT logs**: See if timer fires and packets are sent
4. **Check STAT command**: See if `total_attempted` increases
5. **Check RX node**: See if packets are received

## Most Likely Root Causes

1. **Timer not firing** (60% probability)
2. **Status register polling too slow** (30% probability)
3. **Configuration not applied** (10% probability)

