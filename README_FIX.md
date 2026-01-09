# UART Command-Response Fix - Quick Start

## 🚀 Quick Start (Raspberry Pi 5)

### 1. Find J-Link Serial Numbers

```bash
JLinkExe -ShowEmuList
```

Note the serial numbers for Node A and Node B.

### 2. Flash Node A (TX)

```bash
cd ~/projects/DWM3001C-starter-firmware
chmod +x build-and-flash-tx.sh
./build-and-flash-tx.sh <JLINK_SERIAL_NODE_A>
```

### 3. Flash Node B (RX)

```bash
chmod +x build-and-flash-rx.sh
./build-and-flash-rx.sh <JLINK_SERIAL_NODE_B>
```

### 4. Test

```bash
cd Orchestrator
npm run web
```

Open browser: `http://<RPI_IP>:5000`

## ✅ Expected Results

### Before Fix
```
⚠️  A PING failed - firmware may not be responding
   Error: Command timeout: PNG
```

### After Fix
```
✓ Node A PING successful
✓ Node A is TX_V2
✓ Node B PING successful
✓ Node B is RX_V2
✓ Node A configured successfully
✓ Node B configured successfully
```

## 📚 Documentation

- **[FIX_SUMMARY.md](FIX_SUMMARY.md)** - Complete fix summary
- **[UART_UNSOLICITED_MESSAGES_FIX.md](UART_UNSOLICITED_MESSAGES_FIX.md)** - Detailed technical explanation
- **[BUILD_AND_FLASH_RPI.md](BUILD_AND_FLASH_RPI.md)** - Build and flash guide

## 🔧 What Was Fixed

1. **Removed unsolicited startup messages** - Firmware no longer sends messages during initialization
2. **Fixed double response bug** - RX firmware was sending `OK\r\n\r\n` instead of `OK\r\n`
3. **Optimized UART interrupt** - Removed excessive logging from interrupt handler

## 🐛 Troubleshooting

### PING still fails?

```bash
# Check serial ports
ls -l /dev/ttyUSB*

# Test with minicom
minicom -D /dev/ttyUSB0 -b 115200
# Type: PNG<Enter>
# Should see: OK
```

### LEDs not blinking?

After flash, boards should:
1. Red LED blinks twice (firmware started)
2. Orange LED blinks twice (UART initialized)
3. Blue LED blinks twice (UART ready)
4. All LEDs off (waiting for commands)

### Check debug output

```bash
JLinkRTTClient
```

Should see:
```
=== FIRMWARE STARTING ===
ORCHESTRATOR TX v2.0
OK STARTUP V2
OK DW3000_READY
OK MAIN_LOOP
```

## 📝 Notes

- Startup messages still visible via RTT (debug console), just not sent over UART
- LED behavior unchanged - still provides visual feedback
- No breaking changes to command protocol

## 🎯 Next Steps

After successful flash:
1. Run orchestrator web server: `cd Orchestrator && npm run web`
2. Open web UI: `http://<RPI_IP>:5000`
3. Configure test parameters
4. Start test
5. View results in real-time

## 📞 Support

If issues persist:
1. Check [UART_UNSOLICITED_MESSAGES_FIX.md](UART_UNSOLICITED_MESSAGES_FIX.md) for detailed troubleshooting
2. Check [BUILD_AND_FLASH_RPI.md](BUILD_AND_FLASH_RPI.md) for build instructions
3. Check previous fixes:
   - [UART_PIN_CONFIG_FIX.md](UART_PIN_CONFIG_FIX.md)
   - [UART_ROBUST_INIT_FIX.md](UART_ROBUST_INIT_FIX.md)
   - [GPIO_FIX_SUMMARY.md](GPIO_FIX_SUMMARY.md)
