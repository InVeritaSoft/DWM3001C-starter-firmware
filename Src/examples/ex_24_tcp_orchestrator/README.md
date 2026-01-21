# TCP-like Orchestrator Example

## Overview

This example implements TCP-like reliable communication over UWB, based on `ex_22_orchestrator_v2` with the following enhancements:

- **ACK Packets**: RX node sends ACK packets after receiving each data packet
- **Retransmission Queue**: TX node maintains a queue of un-ACKed packets
- **Timeout Handling**: Retransmit packets if ACK not received within timeout
- **Flow Control**: Sliding window to limit packets in flight
- **TCP-like Statistics**: ACK rate, retransmissions, packets in flight

## Features

### TX Node (tcp_orchestrator_tx.c)
- Maintains retransmission queue (up to 32 packets)
- Tracks ACK status for each packet
- Retransmits on timeout (configurable, default 100ms)
- Maximum retransmission attempts (configurable, default 3)
- Sliding window to limit packets in flight (configurable, default 10)
- Processes ACK packets from RX node

### RX Node (tcp_orchestrator_rx.c)
- Sends ACK packet immediately after receiving data packet
- Handles duplicate packets (sends ACK for duplicates too)
- Tracks received sequence numbers
- TCP-like statistics (ACKs sent, duplicates)

## Packet Structure

### Data Packet (TX → RX)
```c
typedef struct {
    uint32_t seq;        // Sequence number
    uint32_t ack_seq;    // Acknowledged sequence (0 for data packets)
    uint8_t flags;       // TCP flags (0 for data packets)
    uint16_t window;     // Receiver window size
    uint32_t t_local;    // Local timestamp
    uint8_t payload[64]; // Payload data
} uwb_tcp_packet_t;
```

### ACK Packet (RX → TX)
```c
typedef struct {
    uint32_t seq;        // 0 (ACK packets don't have sequence)
    uint32_t ack_seq;    // Acknowledged sequence number
    uint8_t flags;       // TCP_FLAG_ACK (0x01)
    uint16_t window;     // Current receiver window size
    uint32_t t_local;    // Local timestamp
    uint8_t payload[64]; // Empty (ACK packets are small)
} uwb_tcp_packet_t;
```

## Configuration

### TCP-like Parameters

Add to CFG command:
```
CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100 tcp_mode=1 ack_timeout=100 max_retries=3 window_size=10
```

**Parameters:**
- `tcp_mode`: Enable (1) or disable (0) TCP-like mode (default: 1)
- `ack_timeout`: ACK timeout in milliseconds (default: 100ms)
- `max_retries`: Maximum retransmission attempts (default: 3)
- `window_size`: Sliding window size - max packets in flight (default: 10)

## Commands

### Standard Commands (same as orchestrator_v2)
- `PING` / `PNG` - Test connectivity
- `NODE_TYPE` - Get node type (returns `TCP_TX` or `TCP_RX`)
- `START` / `STRT` / `START_TEST` - Start transmission/reception
- `STOP` / `STOP_TEST` - Stop transmission/reception
- `STATS` / `STAT` / `GET_STATS` - Get statistics
- `RESET_STATS` / `RST` - Reset statistics
- `CFG` / `SET_CONFIG` - Configure UWB parameters

### Enhanced STATS Output

**TX Node (TCP mode enabled):**
```
OK STATS sent=XXX attempted=XXX errors=0 timeouts=0 last_err=0 frame_dur=256 
acks_rcvd=XXX acks_missing=XXX retrans=XXX in_flight=XXX
```

**RX Node (TCP mode enabled):**
```
OK STATS rx=XXX lost=XXX crc_err=0 phy_err=0 timeout=0 overrun=0 
rssi_avg=XXX rssi_min=XXX rssi_max=XXX pre_q_avg=XXX pre_q_min=XXX pre_q_max=XXX 
fp_index_avg=XXX evt_crc_good=XXX evt_crc_bad=XXX acks_sent=XXX dup_pkts=XXX
```

## How It Works

### TX Node Flow
1. Send data packet with sequence number
2. Add packet to retransmission queue
3. Wait for ACK from RX node
4. If ACK received: Remove from queue
5. If timeout: Retransmit packet (up to max_retries)
6. If window full: Skip transmission until window has space

### RX Node Flow
1. Receive data packet
2. Check sequence number (detect duplicates)
3. Update statistics
4. **Send ACK packet immediately** (if TCP mode enabled)
5. Re-enable RX for next packet

## Usage

### Enable TCP Orchestrator

**For TX Node:**
Edit `Src/example_selection.h`:
```c
//#define TEST_UART_PING
#define TEST_TCP_ORCHESTRATOR_TX
```

Edit `Src/main.c`:
```c
// extern int uart_ping_test(void); uart_ping_test();
extern int tcp_orchestrator_tx(void); tcp_orchestrator_tx();
```

**For RX Node:**
Edit `Src/example_selection.h`:
```c
//#define TEST_UART_PING
#define TEST_TCP_ORCHESTRATOR_RX
```

Edit `Src/main.c`:
```c
// extern int uart_ping_test(void); uart_ping_test();
extern int tcp_orchestrator_rx(void); tcp_orchestrator_rx();
```

### Testing

1. **Configure both nodes:**
   ```
   CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100 tcp_mode=1 ack_timeout=100 max_retries=3 window_size=10
   ```

2. **Start RX node first:**
   ```
   START
   ```

3. **Start TX node:**
   ```
   START
   ```

4. **Check statistics:**
   ```
   STATS
   ```

5. **Expected behavior:**
   - TX node: `acks_rcvd` should increase as packets are ACKed
   - RX node: `acks_sent` should match `rx` count
   - If packets are lost: TX node will retransmit (`retrans` increases)
   - Window control: TX node `in_flight` should not exceed `window_size`

## TCP-like Features

### ✅ Implemented
- ACK packets from RX to TX
- Retransmission queue on TX
- Timeout and retransmission
- Sliding window flow control
- Duplicate packet detection
- TCP-like statistics

### ❌ Not Implemented (Future)
- Connection management (handshake)
- Ordered delivery (out-of-order reassembly)
- Selective ACK (SACK)
- Congestion control

## Performance Tuning

### Fast Mode (Lower Reliability)
```
CFG ... tcp_mode=1 ack_timeout=50 max_retries=2 window_size=5
```

### Balanced Mode (Default)
```
CFG ... tcp_mode=1 ack_timeout=100 max_retries=3 window_size=10
```

### Reliable Mode (Higher Reliability)
```
CFG ... tcp_mode=1 ack_timeout=200 max_retries=5 window_size=20
```

## Notes

- TCP mode can be disabled by setting `tcp_mode=0` (behaves like orchestrator_v2)
- ACK packets are small (~16 bytes) and sent immediately after data reception
- Retransmission queue size is fixed at 32 packets
- Window size limits how many packets can be in flight simultaneously
- Duplicate packets are detected and ACKed (helps TX node recover)
