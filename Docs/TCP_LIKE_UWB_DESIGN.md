# TCP-like UWB Communication Design

## Overview

This document describes the design for implementing TCP-like reliable communication over UWB using the orchestrator_v2 firmware.

## Current State

The orchestrator_v2 firmware currently has:
- ✅ Sequence numbers in packets
- ✅ Sequence tracking on RX side (detects lost packets)
- ❌ No acknowledgments (ACK)
- ❌ No retransmission
- ❌ No connection management
- ❌ No flow control

## TCP-like Features to Implement

### 1. Acknowledgment (ACK) Packets
- RX node sends ACK packets back to TX node
- ACK contains: last received sequence number, window size
- TX node tracks which packets have been ACKed

### 2. Retransmission
- TX node maintains a retransmission queue
- If ACK not received within timeout, retransmit packet
- Maximum retransmission attempts before giving up

### 3. Connection Management
- Three-way handshake: SYN → SYN-ACK → ACK
- Connection state tracking (IDLE, CONNECTING, CONNECTED, CLOSING, CLOSED)
- Keep-alive mechanism

### 4. Flow Control
- Sliding window protocol
- Window size negotiation
- Backpressure handling

### 5. Ordered Delivery
- Already implemented (sequence numbers)
- Reassembly buffer for out-of-order packets

## Packet Structure

### Data Packet (TX → RX)
```
[Header: 8 bytes]
  - seq: uint32_t      // Sequence number
  - flags: uint8_t     // SYN, ACK, FIN, RST flags
  - window: uint16_t   // Receiver window size
  - checksum: uint8_t  // Simple checksum
  
[Payload: variable]
  - data: uint8_t[]    // User data
```

### ACK Packet (RX → TX)
```
[Header: 8 bytes]
  - ack_seq: uint32_t  // Acknowledged sequence number
  - flags: uint8_t     // ACK flag set
  - window: uint16_t   // Current receiver window size
  - checksum: uint8_t
```

## State Machine

### TX Node States
- **IDLE**: Not connected
- **SYN_SENT**: Sent SYN, waiting for SYN-ACK
- **CONNECTED**: Connection established, can send data
- **FIN_WAIT**: Sent FIN, waiting for FIN-ACK
- **CLOSED**: Connection closed

### RX Node States
- **IDLE**: Not connected
- **LISTEN**: Waiting for SYN
- **SYN_RECEIVED**: Received SYN, sent SYN-ACK
- **CONNECTED**: Connection established, receiving data
- **CLOSE_WAIT**: Received FIN, can still send data
- **CLOSED**: Connection closed

## Implementation Plan

### Phase 1: Basic ACK Mechanism
1. Add ACK packet structure
2. RX sends ACK after receiving data packet
3. TX tracks ACK status
4. Basic timeout and retransmission

### Phase 2: Connection Management
1. Implement three-way handshake
2. Connection state machine
3. Connection commands (CONNECT, DISCONNECT)

### Phase 3: Flow Control
1. Sliding window implementation
2. Window size negotiation
3. Backpressure handling

### Phase 4: Advanced Features
1. Out-of-order packet handling
2. Selective ACK (SACK)
3. Congestion control

## Commands

### New Commands
- `CONNECT` - Initiate connection (TX node)
- `DISCONNECT` - Close connection
- `SEND <data>` - Send data over TCP-like connection
- `RECV` - Get received data from buffer

### Modified Commands
- `START` - Now requires connection first
- `STATS` - Include TCP-like stats (ACK rate, retransmissions, etc.)

## Configuration

### TCP-like Parameters
- `ack_timeout_ms`: Timeout for ACK (default: 100ms)
- `max_retries`: Maximum retransmission attempts (default: 3)
- `window_size`: Sliding window size (default: 10 packets)
- `keepalive_interval_ms`: Keep-alive interval (default: 5000ms)

## Statistics

### New Statistics
- `acks_received`: Number of ACKs received
- `acks_missing`: Number of missing ACKs (timeouts)
- `retransmissions`: Number of retransmitted packets
- `connection_state`: Current connection state
- `window_size`: Current window size
