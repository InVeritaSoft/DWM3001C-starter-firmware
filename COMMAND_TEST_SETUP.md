# Command Test Setup Guide

## Overview

This guide helps you set up a two-node test system:
- **Node A (TX)**: DWM3001CDK + Arduino (transmits UWB packets)
- **Node B (RX)**: DWM3001CDK + Arduino (receives UWB packets)

Commands are sent via Serial Monitor to Arduino, which forwards them to DWM3001CDK.

## Step 1: Flash Arduinos

### For Both Arduinos:

1. Open `Arduino/Command_Test/Command_Test.ino` in Arduino IDE
2. Select **Tools → Board → Arduino Uno**
3. Select **Tools → Port** (choose the correct COM port)
4. Click **Upload**

### Node Identification:

- **Node A Arduino**: Connect **A0 to GND** (identifies as TX node)
- **Node B Arduino**: Leave **A0 floating** or connect to **5V** (identifies as RX node)

## Step 2: Flash DWM3001CDK - Node A (TX)

### 2.1 Enable TX Firmware

Edit `Src/example_selection.h`:
```c
// Comment out other tests:
// #define TEST_UART_PING

// Uncomment:
#define TEST_ORCHESTRATOR_TX_V2
```

Edit `Src/main.c`:
```c
// Comment out:
// extern int uart_ping_test(void); uart_ping_test();

// Uncomment:
extern int orchestrator_tx_v2(void); orchestrator_tx_v2();
```

### 2.2 Build and Flash

```bash
cd /Users/lolibai/Documents/inverita/DWM3001C-starter-firmware
make clean
make build
```

Flash using J-Link:
```bash
cat > /tmp/flash.jlink << 'EOF'
loadfile Output/Common/Exe/dw3000_api.hex
r
g
q
EOF
JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -CommandFile /tmp/flash.jlink
```

## Step 3: Flash DWM3001CDK - Node B (RX)

### 3.1 Enable RX Firmware

Edit `Src/example_selection.h`:
```c
// Comment out:
// #define TEST_ORCHESTRATOR_TX_V2

// Uncomment:
#define TEST_ORCHESTRATOR_RX_V2
```

Edit `Src/main.c`:
```c
// Comment out:
// extern int orchestrator_tx_v2(void); orchestrator_tx_v2();

// Uncomment:
extern int orchestrator_rx_v2(void); orchestrator_rx_v2();
```

### 3.2 Build and Flash

```bash
make clean
make build
```

Flash using J-Link (same command as Step 2.2)

## Step 4: Wiring

### Node A (TX) Wiring:
```
DWM3001CDK J10          Arduino
─────────────────      ──────────
Pin 19 (TX/GPIO27)  →  D8 (RX)  [GPIO27_PIN19_TX]
Pin 10 (RX/GPIO15)  ←  D9 (TX)
Pin 6  (GND)        →  GND

Arduino A0 → GND (for Node A identification)
```

### Node B (RX) Wiring:
```
DWM3001CDK J10          Arduino
─────────────────      ──────────
Pin 19 (TX/GPIO27)  →  D8 (RX)  [GPIO27_PIN19_TX]
Pin 10 (RX/GPIO15)  ←  D9 (TX)
Pin 6  (GND)        →  GND

Arduino A0 → Floating or 5V (for Node B identification)
```

## Step 5: Testing

### 5.1 Open Serial Monitors

Open two Serial Monitor windows:
- **Node A Serial Monitor**: Connect to Node A Arduino (115200 baud)
- **Node B Serial Monitor**: Connect to Node B Arduino (115200 baud)

### 5.2 Basic Connectivity Test

**On Node A Serial Monitor:**
```
PING
```
Expected: `[RECV] OK`

**On Node B Serial Monitor:**
```
PING
```
Expected: `[RECV] OK`

### 5.3 Check Node Types

**On Node A:**
```
NODE_TYPE
```
Expected: `[RECV] OK NODE_TYPE=TX_V2`

**On Node B:**
```
NODE_TYPE
```
Expected: `[RECV] OK NODE_TYPE=RX_V2`

### 5.4 Configure Both Nodes

**On Node A (TX):**
```
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
```
Expected: `[RECV] OK CONFIG`

**On Node B (RX):**
```
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
```
Expected: `[RECV] OK CONFIG`

### 5.5 Start Test

**On Node B (RX) - Start receiving first:**
```
START
```
Expected: `[RECV] OK START`

**On Node A (TX) - Then start transmitting:**
```
START
```
Expected: `[RECV] OK START`

### 5.6 Check Statistics

**On Node A (TX):**
```
STATS
```
Expected: `[RECV] OK STATS sent=XXX attempted=XXX errors=0 timeouts=0 ...`

**On Node B (RX):**
```
STATS
```
Expected: `[RECV] OK STATS rx=XXX lost=XXX crc_err=0 phy_err=0 ...`

### 5.7 Stop Test

**On Node A (TX):**
```
STOP
```
Expected: `[RECV] OK STOP`

**On Node B (RX):**
```
STOP
```
Expected: `[RECV] OK STOP`

## Troubleshooting

### Arduino Not Responding
- Check wiring (D8/D9 connections)
- Verify Arduino is powered
- Check Serial Monitor baud rate (115200)
- Verify node identification (A0 connection)

### DWM3001CDK Not Responding
- Check wiring (J10 Pin 19 and Pin 10)
- Verify DWM3001CDK is powered
- Check that correct firmware is flashed (TX vs RX)
- Try `PING` command first

### No Packets Received
- Ensure both nodes are configured (`CFG` command)
- Ensure both nodes are started (`START` command on both)
- Check that nodes are within range
- Verify same channel configuration on both nodes

### Wrong Node Type
- Node A: Ensure A0 is connected to GND
- Node B: Ensure A0 is floating or connected to 5V
- Check startup message in Serial Monitor

## Command Reference

| Command | Short Form | Description |
|---------|-----------|-------------|
| `PING` | `PNG` | Test connectivity |
| `NODE_TYPE` | - | Get node type |
| `START` | `STRT` or `START_TEST` | Start TX/RX |
| `STOP` | `STOP_TEST` | Stop TX/RX |
| `STATS` | `STAT` or `GET_STATS` | Get statistics |
| `CONFIG` | `CFG` or `SET_CONFIG` | Configure UWB |
| `RESET_STATS` | `RST` | Reset statistics |

## Configuration Example

```
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100
```

**Parameters:**
- `ch`: Channel (5 or 9)
- `rate`: Data rate (`850k` or `6m8`)
- `pl`: Preamble length (64, 128, 256, 512, 1024)
- `len`: Payload length (bytes)
- `pwr_ref`: Reference TX power (hex)
- `boost`: Power boost (0-30, 0.1dB steps)
- `rate_hz`: Packet rate (packets per second)


  - PING
  - NODE_TYPE
  - START
  - STOP
  - STATS
  - CONFIG <params>
  - RESET_STATS