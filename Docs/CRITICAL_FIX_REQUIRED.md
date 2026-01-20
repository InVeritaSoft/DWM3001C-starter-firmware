# ⚠️ CRITICAL FIX REQUIRED: RS-485 DE Pin Control

## 🚨 URGENT: Firmware Not Responding to Commands

Your firmware is currently **not responding to PING commands** because the **RS-485 transceiver direction control (DE pin) is missing**.

## Quick Summary

**Problem:** RS-485 transceiver stuck in transmit mode or high-impedance state
**Solution:** Add GPIO 13 control for RS-485 DE (Driver Enable) pin
**Impact:** **CRITICAL** - Without this fix, firmware cannot receive commands

## 🚀 Quick Fix (5 minutes)

### Step 1: Pull Latest Changes
```bash
cd ~/projects/DWM3001C-starter-firmware
git pull origin uwb-test-rig
```

### Step 2: Flash Both Nodes
```bash
# Find J-Link serial numbers
JLinkExe -ShowEmuList

# Flash Node A (TX)
./build-and-flash-tx.sh <JLINK_SERIAL_NODE_A>

# Flash Node B (RX)
./build-and-flash-rx.sh <JLINK_SERIAL_NODE_B>
```

### Step 3: Test
```bash
cd Orchestrator
npm run web
```

## ✅ Expected Results

### Before Fix (Current State)
```
[RS485 TX] /dev/ttyUSB0: PNG (5 bytes including \r\n)
❌ Command timeout: PNG - No response received
```

### After Fix
```
[RS485 TX] /dev/ttyUSB0: PNG (5 bytes including \r\n)
[RS485 RX] /dev/ttyUSB0: OK
✅ Node A PING successful
✅ Node A is TX_V2
```

## 📋 What Changed

### Files Modified
1. **`Src/custom_board.h`**
   - Added `RS485_DE_PIN` definition (GPIO 13)

2. **`Src/examples/ex_22_orchestrator_v2/orchestrator_rx_v2.c`**
   - Initialize DE pin in `uart_init()`
   - Control DE pin in `send_response()`

3. **`Src/examples/ex_22_orchestrator_v2/orchestrator_tx_v2.c`**
   - Initialize DE pin in `uart_init()`
   - Control DE pin in `send_response()`

### How It Works

**RS-485 DE (Driver Enable) Pin:**
- **LOW (default)**: Receive mode - firmware can receive commands
- **HIGH (transmit)**: Transmit mode - firmware can send responses
- **LOW (after TX)**: Back to receive mode - ready for next command

**Timing:**
```
Command arrives → DE=LOW (receive) → Process command → 
DE=HIGH (transmit) → Send response → DE=LOW (receive) → Wait for next command
```

## 🔧 Hardware Requirements

### RS-485 Transceiver Wiring
```
DWM3001C          RS-485 Transceiver
---------         ------------------
GPIO 14 (TX)  --> DI (Driver Input)
GPIO 15 (RX)  <-- RO (Receiver Output)
GPIO 13 (DE)  --> DE (Driver Enable)    ⚠️ NEW - CRITICAL
GPIO 13 (DE)  --> RE (Receiver Enable)  ⚠️ NEW - CRITICAL
GND           --> GND
3.3V          --> VCC
```

**⚠️ IMPORTANT:** If GPIO 13 is not connected to your RS-485 transceiver DE pin, you **MUST** connect it now!

## 🐛 Troubleshooting

### Q: Still no response after flashing?

**A: Check hardware connections:**
```bash
# Verify GPIO 13 is connected to RS-485 DE pin
# Measure voltage on GPIO 13:
# - Should be 0V when idle (receive mode)
# - Should pulse to 3.3V when transmitting
```

### Q: Which GPIO pin should I use for DE?

**A: GPIO 13 (P0.13) is recommended:**
- Available on J10 Pin 6
- Not used by other peripherals
- Easy to access

If you need to use a different pin, edit `custom_board.h`:
```c
#define RS485_DE_PIN NRF_GPIO_PIN_MAP(0, 12)  // Change to your pin
```

### Q: My transceiver has separate DE and RE pins?

**A: Tie them together or use inverter:**
```
Option 1 (Tie together):
GPIO 13 --> DE
GPIO 13 --> RE

Option 2 (Inverter):
GPIO 13 --> DE
GPIO 13 --> NOT gate --> RE
```

## 📚 Documentation

- **[RS485_DE_PIN_FIX.md](RS485_DE_PIN_FIX.md)** - Detailed technical explanation
- **[UART_UNSOLICITED_MESSAGES_FIX.md](UART_UNSOLICITED_MESSAGES_FIX.md)** - Previous fix (unsolicited messages)
- **[BUILD_AND_FLASH_RPI.md](BUILD_AND_FLASH_RPI.md)** - Build and flash guide

## 🎯 Priority

**CRITICAL** - This fix is **required** for any RS-485 communication to work. Without it:
- ❌ Firmware cannot receive commands
- ❌ PING commands timeout
- ❌ Configuration fails
- ❌ Tests cannot run

## ⏱️ Time Estimate

- **Pull changes**: 30 seconds
- **Flash Node A**: 2 minutes
- **Flash Node B**: 2 minutes
- **Test**: 30 seconds
- **Total**: ~5 minutes

## 🆘 Need Help?

If you encounter issues:
1. Check hardware connections (GPIO 13 to RS-485 DE pin)
2. Verify RS-485 transceiver type (MAX485, MAX3485, SN65HVD72, etc.)
3. Check RTT debug output: `JLinkRTTClient`
4. Test with direct serial: `minicom -D /dev/ttyUSB0 -b 115200`
5. Read detailed documentation: [RS485_DE_PIN_FIX.md](RS485_DE_PIN_FIX.md)

## ✨ After This Fix

Once applied, you should see:
- ✅ PING commands work
- ✅ NODE_TYPE commands work
- ✅ Configuration commands work
- ✅ START/STOP commands work
- ✅ Full orchestrator functionality

**This is the final critical fix needed for RS-485 communication!**
