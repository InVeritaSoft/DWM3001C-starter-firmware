# Firmware Comparison: Orchestrator v2 vs Zephyr

## Critical Difference: UART GPIO Pins

### Orchestrator v2 Firmware (`Src/examples/ex_22_orchestrator_v2/`)
- **GPIO Pins**: **14 (TX) and 15 (RX)**
- **J10 Mapping**: 
  - GPIO 14 (P0.14) = J10 Pin 8 (TXD0) ✅ **CONFIRMED**
  - GPIO 15 (P0.15) = J10 Pin 10 (RXD0) ✅ **CONFIRMED**
- **UART Library**: nRF SDK `app_uart` (interrupt-driven)
- **PNG Response**: `OK` (not "OK PONG")
- **Status**: ✅ **WORKING** - Pins match hardware

### Zephyr Firmware (`Zephyr/`)
- **GPIO Pins**: **8 (TX) and 10 (RX)**
- **J10 Mapping**: 
  - GPIO 8 (P0.08) = J10 Pin 8 (TXD) ⚠️ **MAY NOT MATCH**
  - GPIO 10 (P0.10) = J10 Pin 10 (RXD) ⚠️ **MAY NOT MATCH**
- **UART Library**: Zephyr RTOS (polling mode)
- **PNG Response**: `OK PONG`
- **Status**: ❌ **NOT RESPONDING** - Pins may be wrong

## The Problem

If you're testing with PNG commands and getting no response, you likely have:

1. **Zephyr firmware flashed** (GPIO 8/10) but **hardware connected to GPIO 14/15**
2. **Wrong firmware for your hardware setup**

## Solution

### Option 1: Use Orchestrator v2 Firmware (Recommended)

This firmware uses the **correct GPIO pins** (14/15) that match J10 Pin 8/10:

```bash
# Build orchestrator v2 firmware
# (Check your build system for the correct commands)
```

**Advantages:**
- ✅ GPIO pins match hardware (14/15 = J10 Pin 8/10)
- ✅ Interrupt-driven UART (more reliable)
- ✅ Already tested and working

### Option 2: Fix Zephyr Firmware Pin Configuration

Update `Zephyr/dwm3001cdk.overlay` to use GPIO 14/15:

```dts
&uart0 {
	status = "okay";
	current-speed = <115200>;
	
	/* Change to match orchestrator v2 pins */
	tx-pin = <14>;   // P0.14 - J10 Pin 8 (TXD0) - CONFIRMED
	rx-pin = <15>;   // P0.15 - J10 Pin 10 (RXD0) - CONFIRMED
};
```

Then rebuild and flash:
```bash
cd Zephyr
west build -b decawave_dwm3001cdk -- -DNODE_TYPE=A -t clean
west build -b decawave_dwm3001cdk -- -DNODE_TYPE=A
west flash --runner jlink
```

## Which Firmware Should You Use?

**Use Orchestrator v2 if:**
- You need immediate functionality
- Your hardware is connected to J10 Pin 8/10
- You want proven, working firmware

**Use Zephyr if:**
- You're migrating to Zephyr RTOS
- You need Zephyr-specific features
- You're willing to debug pin configuration

## Quick Test

To verify which firmware is running:

1. **Check startup messages:**
   - Orchestrator v2: "OK STARTUP V2"
   - Zephyr: "UWB Test Node Firmware Starting..."

2. **Check PNG response:**
   - Orchestrator v2: `OK` (just "OK")
   - Zephyr: `OK PONG`

3. **Check GPIO pins in code:**
   - Orchestrator v2: GPIO 14/15 (in `custom_board.h`)
   - Zephyr: GPIO 8/10 (in `dwm3001cdk.overlay`)

## Recommendation

**For your current issue (no PNG response):**

1. **Verify which firmware is flashed** - Check startup messages
2. **If Zephyr is flashed** - Update overlay to use GPIO 14/15
3. **Or switch to Orchestrator v2** - It already has correct pins

The orchestrator v2 firmware is confirmed to work with J10 Pin 8/10 because it uses GPIO 14/15, which are the default UART pins on the DWM3001CDK.
