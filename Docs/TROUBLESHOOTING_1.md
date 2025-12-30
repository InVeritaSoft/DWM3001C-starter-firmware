# Troubleshooting Guide: No Serial Responses

## Current Status
- **Node A (COM17)**: Should be TX v2, but not responding
- **Node B (COM18)**: Should be RX v2, but currently configured as TX v2

## Diagnostic Results
- Commands are being sent correctly (hex output verified)
- No responses received from either board
- No startup messages received (`OK STARTUP V2`, `OK DW3000_READY`, `OK MAIN_LOOP`)

## Step-by-Step Troubleshooting

### 1. Verify Firmware Configuration

**Check which firmware is currently configured:**

```powershell
# Check example_selection.h
cat Src\example_selection.h | findstr "TEST_ORCHESTRATOR"

# Check main.c
cat Src\main.c | findstr "orchestrator"
```

**Expected:**
- Node A: `TEST_ORCHESTRATOR_TX_V2` defined, `orchestrator_tx_v2()` uncommented
- Node B: `TEST_ORCHESTRATOR_RX_V2` defined, `orchestrator_rx_v2()` uncommented

### 2. Rebuild and Flash Correct Firmware

**For Node A (TX):**
```powershell
.\build-and-flash-tx.ps1
```

**For Node B (RX):**
```powershell
.\build-and-flash-rx.ps1
```

**Verify the scripts are using v2:**
- `build-and-flash-tx.ps1` should call `set-node-tx-v2.ps1`
- `build-and-flash-rx.ps1` should call `set-node-rx-v2.ps1`

### 3. Test Serial Connection

**After flashing, test each board:**
```powershell
cd Orchestrator
npm run test-serial COM17  # Test Node A
npm run test-serial COM18  # Test Node B
```

**What to look for:**
- Startup messages: `OK STARTUP V2`, `OK DW3000_READY`, `OK MAIN_LOOP`
- Response to PING: `OK`
- Response to NODE_TYPE: `OK NODE_TYPE=TX_V2` or `OK NODE_TYPE=RX_V2`

### 4. Check LED Behavior

**On power-on:**
- Red LED should blink briefly (startup)
- Blue LED should turn on (UART initialized)
- LEDs should blink when commands are received

**When commands are sent:**
- **Orange LED**: Should blink when command is received
- **Green LED**: Should blink when response is sent
- **If no LEDs blink**: Firmware may not be running or not receiving commands

### 5. Hardware Checks

**RS-485 Connection:**
- Verify RS-485 transceiver is connected
- Check wiring:
  - A+ line (differential positive)
  - B- line (differential negative)
  - GND (ground)
- Termination resistors: 120Ω at each end of the bus
- Transceiver enable/DE pins configured correctly

**Serial Port:**
- Verify COM port numbers (COM17 for Node A, COM18 for Node B)
- Check baud rate: 115200
- Ensure no other software is using the ports
- Try unplugging and replugging USB cables

### 6. Test with Direct Serial Terminal

**Using PuTTY or Tera Term:**
1. Open serial connection
2. Configure: 115200 baud, 8N1, no flow control
3. Send: `PNG` followed by Enter (sends `PNG\r\n`)
4. Should receive: `OK\r\n`
5. Watch LEDs: orange on TX, green on RX

### 7. Verify Firmware is Actually Running

**Check if firmware starts:**
- Power cycle the board
- Watch for LED patterns
- Red LED blinking rapidly = error condition
- Blue LED on = UART initialized
- Startup messages should appear in serial terminal

**If firmware doesn't start:**
- Check if firmware was flashed successfully
- Verify flash process completed without errors
- Try reflashing the firmware
- Check board power supply

## Common Issues and Solutions

### Issue: No responses at all
**Possible causes:**
1. Wrong firmware flashed (not orchestrator v2)
2. Firmware crashed during initialization
3. RS-485 transceiver not working
4. Wiring issue (A+/B- swapped, missing GND)
5. Baud rate mismatch

**Solutions:**
- Rebuild and flash firmware
- Check LED behavior on power-on
- Test with direct serial terminal
- Verify RS-485 hardware connections

### Issue: Startup messages appear but commands don't work
**Possible causes:**
1. Command parsing issue
2. UART buffer overflow
3. Timing issue

**Solutions:**
- Check firmware logs
- Verify command format (`PNG\r\n`, not just `PNG`)
- Increase UART buffer size if needed

### Issue: One board works, other doesn't
**Possible causes:**
1. Wrong firmware on one board
2. Hardware issue with one board
3. Port configuration issue

**Solutions:**
- Verify firmware configuration for each board
- Swap boards to isolate hardware vs firmware issue
- Check individual board connections

## Quick Verification Checklist

- [ ] Both boards have correct firmware (TX v2 and RX v2)
- [ ] Firmware was flashed successfully
- [ ] Boards power on and LEDs behave correctly
- [ ] Startup messages appear in serial terminal
- [ ] RS-485 hardware is connected correctly
- [ ] Serial ports are correct (COM17/COM18)
- [ ] Baud rate is 115200
- [ ] No other software using the ports
- [ ] Direct serial terminal test works
- [ ] LED blinks when commands are sent

## Next Steps

1. **Fix Node B configuration**: Ensure it's set to RX v2, not TX v2
2. **Rebuild and flash both boards** with correct firmware
3. **Test serial connection** with diagnostic script
4. **Check LED behavior** to verify firmware is running
5. **Verify hardware connections** if LEDs don't blink

