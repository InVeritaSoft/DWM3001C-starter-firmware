# Orchestrator Example - Build and Flash Guide

## Overview

This example provides two firmware variants:
- **TX Variant (Node A)**: Transmits UWB packets periodically
- **RX Variant (Node B)**: Receives UWB packets and collects statistics

Both variants communicate with the Orchestrator via RS-485 using UART.

## Pin Configuration

### UART Pins (RS-485 Communication)
- **RX Pin**: GPIO 15 (P0.15) - Pin 10 on J10
- **TX Pin**: GPIO 14 (P0.14) - Pin 8 on J10
- **Baud Rate**: 115200

**Note**: Pins 8 and 10 are used for:
- **Pin 8**: DW3000_IRQ_Pin (interrupt pin for UWB chip)
- **Pin 10**: DW3000_CS_Pin (SPI chip select for UWB chip)

The UART for RS-485 communication uses **GPIO 15 (RX) and GPIO 14 (TX)**, which correspond to **Pin 10 and Pin 8 on J10 connector**.

## Building and Flashing

### Step 1: Build TX Variant (Node A)

1. Edit `Src/example_selection.h`:
   ```c
   // Comment out other examples:
   //#define TEST_READING_DEV_ID
   
   // Uncomment TX variant:
   #define TEST_ORCHESTRATOR_TX
   ```

2. Edit `Src/main.c`:
   ```c
   // Comment out other examples:
   // extern int read_dev_id(void); read_dev_id();
   
   // Uncomment TX variant:
   extern int orchestrator_tx(void); orchestrator_tx();
   ```

3. Build:
   ```bash
   make build
   ```

4. Flash to Node A board:
   ```bash
   make flash
   ```

### Step 2: Build RX Variant (Node B)

1. Edit `Src/example_selection.h`:
   ```c
   // Comment out TX variant:
   //#define TEST_ORCHESTRATOR_TX
   
   // Uncomment RX variant:
   #define TEST_ORCHESTRATOR_RX
   ```

2. Edit `Src/main.c`:
   ```c
   // Comment out TX variant:
   // extern int orchestrator_tx(void); orchestrator_tx();
   
   // Uncomment RX variant:
   extern int orchestrator_rx(void); orchestrator_rx();
   ```

3. Build:
   ```bash
   make build
   ```

4. Flash to Node B board:
   ```bash
   make flash
   ```

**Important**: You need to build and flash **separately** for each board. The firmware is compiled with either TX or RX code, not both.

## LED Behavior

The firmware uses `dwt_setleds()` which controls the **DW3000 UWB chip's internal LEDs**, not the board LEDs.

- **On startup**: LEDs will blink once during initialization
- **During operation**: LEDs may flash during UWB activity (TX/RX events)
- **Board LEDs**: The DWM3001CDK board LEDs (LED_1, LED_2, LED_3, LED_4) are not directly controlled by this firmware

## RS-485 Connection

Connect the RS-485 transceiver to:
- **A+**: Connect to RS-485 A+ line
- **B-**: Connect to RS-485 B- line  
- **GND**: Connect to ground
- **UART RX**: Connect to GPIO 15 (P0.15) - Pin 10 on J10
- **UART TX**: Connect to GPIO 14 (P0.14) - Pin 8 on J10
- **DE/RE**: Connect to control RS-485 direction (if needed)

The firmware communicates at **115200 baud** with the Orchestrator.

## Testing

After flashing:

1. **Connect RS-485** from each board to the Orchestrator (RPi 5)
2. **Power on** both boards
3. **Run the Orchestrator**:
   ```bash
   cd Orchestrator
   npm start
   ```

The Orchestrator will:
- Send `PNG` (PING) commands to verify connectivity
- Configure both nodes with `CFG` commands
- Start tests with `STRT` commands
- Collect statistics with `STAT` commands

## Troubleshooting

### UART Not Working
- Verify pins 15 (RX) and 19 (TX) are connected correctly
- Check RS-485 transceiver wiring (A+, B-, GND)
- Verify baud rate is 115200
- Check that RS-485 termination resistors are present (120Ω at each end)

### Wrong Variant Flashed
- Make sure you build and flash separately for each board
- Check `example_selection.h` has the correct `#define` uncommented
- Check `main.c` has the correct function call uncommented

### LEDs Not Flashing
- The firmware controls DW3000 internal LEDs, not board LEDs
- Board LEDs may not flash unless you add code to control them
- Check UWB activity with the Orchestrator's statistics

