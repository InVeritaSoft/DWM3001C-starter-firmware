# J10 Connector Pin to GPIO Mapping Reference

## Physical Pin Numbers (Square Pins on J10 Connector)

This document maps the **physical J10 connector pin numbers** (the square pins you see on the board) to **nRF52833 GPIO pin numbers** (P0.XX).

### Complete J10 Pinout (from DWM3001CDK Quick Start Guide)

| J10 Pin | Net Name | GPIO Pin (P0.XX) | Current Usage | Notes |
|---------|----------|------------------|---------------|-------|
| 1 | (NC) | - | No Connect | ❌ Don't use for power! |
| 2 | 5V0 | - | Power (5V) | ✅ Use for RS-485 VCC |
| 3 | SDA_RPI | ? | I2C SDA | - |
| 4 | 5V0 | - | Power (5V) | ✅ Use for RS-485 VCC |
| 5 | SCL_RPI | ? | I2C SCL | - |
| 6 | GND | - | Ground | ✅ Use for RS-485 GND |
| 7 | (NC) | - | No Connect | - |
| **8** | **TXD0/GPIO14** | **P0.14** | **UART TX (Default)** | ✅ **Default UART TX pin** |
| 9 | GND | - | Ground | - |
| **10** | **RXD0/GPIO15** | **P0.15** | **UART RX** | ✅ **Confirmed: Your RX pin** |
| 11 | (NC) | - | No Connect | - |
| 12 | RESET | ? | Reset | - |
| **13** | **GPIO27/GPIO_GEN2** | **P0.27** | **GPIO 27** | ⚠️ **Note: GPIO27 is actually on Pin 19** |
| 14 | GND | - | Ground | - |
| **15** | **GPIO_RPI** | **P0.15** | **GPIO 15** | ✅ **Same as Pin 10 (RXD0)** |
| 16 | (NC) | - | No Connect | - |
| 17 | (NC) | - | No Connect | - |
| 18 | (NC) | - | No Connect | - |
| **19** | **GPIO27/GPIO_GEN2** | **P0.27** | **GPIO 27** | ✅ **GPIO27_PIN19_TX - Confirmed TX pin** |

## Current Firmware Configuration

### Software Configuration (in `custom_board.h`):
```c
#define RX_PIN_NUMBER  16  // GPIO 16 (P0.16) - Testing alternative RX pin
#define TX_PIN_NUMBER  15  // GPIO 15 (P0.15) - Confirmed working for TX
```

### Your Hardware Connections (CONFIRMED BY TESTING):
- **TXD (RS-485)** → **J10 Pin 19** → **GPIO 27 (P0.27)** ✅ **GPIO27_PIN19_TX - Confirmed by testing**
- **RXD (RS-485)** → **J10 Pin 10** → **GPIO 15 (P0.15)** ✅ Confirmed (RXD0/GPIO15)

### Alternative UART Pins (from schematic):
- **J10 Pin 8** → **GPIO 14 (P0.14)** = **TXD0** (Default UART TX)
- **J10 Pin 10** → **GPIO 15 (P0.15)** = **RXD0** (Default UART RX)

## Testing Results Summary

| Configuration | TX Pin | RX Pin | Result |
|---------------|--------|--------|--------|
| **Current (CONFIRMED)** | **GPIO 27 (Pin 19)** | **GPIO 15 (Pin 10)** | ✅ **TX works (GPIO27_PIN19_TX)**<br>✅ **RX works (GPIO15)** |

## Key Findings (CONFIRMED BY TESTING)

1. **GPIO 15 (P0.15)** = J10 Pin 10/15 = Works for **RX** ✅
2. **GPIO 27 (P0.27)** = **J10 Pin 19** = Works for **TX only** ✅ **GPIO27_PIN19_TX** (RX doesn't work) ❌
3. **⚠️ IMPORTANT**: GPIO27 is on **J10 Pin 19**, NOT Pin 13 as originally documented

## Problem Identified

**Hardware/Software Mismatch:**
- Your **hardware TXD** is connected to **J10 Pin 13** (GPIO 27)
- But **software TX=15** works (sends on GPIO 15 = J10 Pin 10 or Pin 15)
- This means startup messages go to **GPIO 15**, not **GPIO 27** (Pin 13)!

**Confirmed Pin Mapping (by testing):**
- **J10 Pin 8** = **P0.14** = **TXD0** (Default UART TX pin)
- **J10 Pin 10** = **P0.15** = **RXD0** (Default UART RX pin) ✅ **Confirmed RX pin**
- **J10 Pin 13** = **P0.27** = **GPIO27** (⚠️ **Note: GPIO27 is actually on Pin 19**)
- **J10 Pin 15** = **P0.15** = **GPIO_RPI** (Same as Pin 10, also P0.15)
- **J10 Pin 19** = **P0.27** = **GPIO27** ✅ **GPIO27_PIN19_TX - Confirmed TX pin**

## Solutions

### Option 1: Use Default UART Pins (Recommended - Easiest)
Move your **RS-485 TXD** wire from **J10 Pin 13** to **J10 Pin 8**:
- **TXD (RS-485)** → **J10 Pin 8** → **GPIO 14 (P0.14)** = **TXD0**
- **RXD (RS-485)** → **J10 Pin 10** → **GPIO 15 (P0.15)** = **RXD0**
- Set software: `TX_PIN_NUMBER = 14`, `RX_PIN_NUMBER = 15`

### Option 2: Use GPIO 15 for Both (Current Test)
Keep **RXD** on **J10 Pin 10** (GPIO 15), move **TXD** to **J10 Pin 10 or Pin 15**:
- **TXD (RS-485)** → **J10 Pin 10 or Pin 15** → **GPIO 15 (P0.15)**
- **RXD (RS-485)** → **J10 Pin 10** → **GPIO 15 (P0.15)**
- ⚠️ **Problem**: Can't use same pin for TX and RX simultaneously!

### Option 3: Use GPIO 27 for TX (✅ CURRENT CONFIGURATION - CONFIRMED)
Keep **TXD** on **J10 Pin 19** (GPIO 27), keep **RXD** on **J10 Pin 10** (GPIO 15):
- **TXD (RS-485)** → **J10 Pin 19** → **GPIO 27 (P0.27)** ✅ **GPIO27_PIN19_TX**
- **RXD (RS-485)** → **J10 Pin 10** → **GPIO 15 (P0.15)**
- Set software: `TX_PIN_NUMBER = 27`, `RX_PIN_NUMBER = 15`

## LED Pin Mappings (Important for Diagnostics)

From the schematic:
- **D9 LED** → **P0.14** (GPIO 14) = Also **TXD0** on J10 Pin 8
- **D10 LED** → **P0.22** (GPIO 22)
- **D11 LED** → **P0.04** (GPIO 4)
- **D12 LED** → **P0.09** (GPIO 9) ⚠️ **Note**: Your code may reference D12 as GPIO 14, but schematic shows D12 on P0.09
- **D13 (Red/Green LED)** → Red on **P0.27** (GPIO 27), Green on **P0.07** (GPIO 7)

**Diagnostic Output Location:**
- **"Ping by count"** = Diagnostic messages in **RTT logs** (via SEGGER J-Link)
- **"Square pin"** = Physical pins showing UART activity (square wave signals):
  - **J10 Pin 8** (P0.14) = TXD0 - shows TX activity
  - **J10 Pin 10** (P0.15) = RXD0 - shows RX activity
  - **J10 Pin 19** (P0.27) = **GPIO27_PIN19_TX** - Your TX pin - shows TX activity when configured ✅

## Notes

- **J10 Pin numbers** (1, 2, 3... 19) = Physical connector pins (square pins)
- **GPIO numbers** (P0.15, P0.27, etc.) = nRF52833 microcontroller pins
- **Not all J10 pins map to GPIO pins** - Some are power (5V, GND), some are NC (No Connect)
- **GPIO pin numbers** are what you use in software (`#define TX_PIN_NUMBER 15`)
