# RTT Debug Output Troubleshooting

## Problem: No RTT Output

If you're not seeing any output via RTT, follow these steps:

## Step 1: Verify Board Connection

1. **Check USB Connection:**
   - Ensure J9 (USB port) is connected to your computer
   - Try unplugging and replugging the USB cable
   - Check Device Manager (Windows) for "SEGGER J-Link" device

2. **Check Board Power:**
   - Board should be powered via USB (J9)
   - Check if LEDs on the board are lit/blinking
   - If no LEDs, board may not be powered

## Step 2: Verify Firmware is Flashed

Run this command to check if firmware is on the board:

```powershell
.\flash.ps1 -NodeType RX
```

Or check if firmware file exists:
```powershell
Test-Path Output\Common\Exe\dw3000_api.hex
```

## Step 3: Check RTT Logger Connection

1. **Start RTT Logger:**
   ```powershell
   .\stream-debug-logs.ps1
   ```

2. **Look for these messages:**
   - ✅ "Connected to: SEGGER J-Link"
   - ✅ "Searching for RTT Control Block...OK"
   - ✅ "2 up-channels found"
   - ✅ "Selected RTT Channel description: Index: 0, Name: Terminal"

3. **If you see "Searching for RTT Control Block..." but it hangs:**
   - Firmware might not be running
   - Press reset button (SW1) on the board
   - Or power cycle the board

## Step 4: Force Firmware Restart

While RTT logger is running:

1. **Press Reset Button (SW1)** on the board
2. **Or power cycle:** Unplug USB, wait 2 seconds, plug back in
3. **Watch for output** - you should see startup messages immediately

## Step 5: Verify Firmware Configuration

Check that the correct firmware variant is configured:

```powershell
# Check example_selection.h
Select-String -Path "Src\example_selection.h" -Pattern "TEST_ORCHESTRATOR_RX_V2"

# Should show:
# #define TEST_ORCHESTRATOR_RX_V2
```

## Step 6: Test with Simple Example

Try flashing a simpler example that definitely outputs:

1. Edit `Src/example_selection.h`:
   ```c
   // Comment out RX v2:
   //#define TEST_ORCHESTRATOR_RX_V2
   
   // Uncomment simple example:
   #define TEST_READING_DEV_ID
   ```

2. Edit `Src/main.c`:
   ```c
   // Comment out RX v2:
   // extern int orchestrator_rx_v2(void); orchestrator_rx_v2();
   
   // Uncomment simple example:
   extern int read_dev_id(void); read_dev_id();
   ```

3. Rebuild and flash:
   ```powershell
   .\build.ps1
   .\flash.ps1
   ```

4. Run RTT logger and press reset - you should see output

## Step 7: Check USB Device Sharing (Windows)

If using WSL2, ensure USB device is shared:

```powershell
# Check if usbipd is installed
usbipd list

# If J-Link is listed but not attached, attach it:
usbipd attach --wsl --busid <busid>
```

## Common Issues

### Issue: RTT Logger connects but no data
**Solution:** Firmware is running but not outputting. Press reset button while logger is running.

### Issue: "Searching for RTT Control Block..." hangs
**Solution:** Firmware not running. Flash firmware or press reset.

### Issue: "No devices found"
**Solution:** USB not connected or not shared with WSL2. Check USB connection and run `.\setup-usb.ps1`

### Issue: RTT Logger exits immediately
**Solution:** Check if board is powered and firmware is flashed.

## Expected Output

When working correctly, you should see:

```
ORCHESTRATOR RX v2.0
OK STARTUP V2
OK DW3000_READY
OK MAIN_LOOP
```

Or for TX variant:
```
ORCHESTRATOR TX v2.0
UART INIT FAILED
OK STARTUP V2
OK DW3000_READY
OK MAIN_LOOP
```

