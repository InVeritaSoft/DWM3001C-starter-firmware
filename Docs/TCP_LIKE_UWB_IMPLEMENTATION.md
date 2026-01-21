# TCP-like UWB Implementation Plan

## Priority Features

### Phase 1: Basic Reliability (HIGH PRIORITY)
**Goal**: Ensure packets are delivered reliably with acknowledgments and retransmission.

**Features**:
1. **ACK Packets**: RX node sends ACK after receiving each data packet
2. **Retransmission Queue**: TX node maintains queue of un-ACKed packets
3. **Timeout Handling**: Retransmit if ACK not received within timeout
4. **ACK Tracking**: Track which packets have been acknowledged

**Implementation**:
- Add ACK packet structure (small, 8-12 bytes)
- RX sends ACK immediately after receiving data packet
- TX maintains retransmission queue (circular buffer)
- Retransmit on timeout (configurable, default 100ms)
- Maximum retransmission attempts (default 3)

### Phase 2: Connection Management (MEDIUM PRIORITY)
**Goal**: Establish and manage connections between nodes.

**Features**:
1. **Three-way Handshake**: SYN → SYN-ACK → ACK
2. **Connection States**: IDLE, CONNECTING, CONNECTED, CLOSING, CLOSED
3. **Connection Commands**: CONNECT, DISCONNECT
4. **Keep-alive**: Periodic keep-alive packets

**Implementation**:
- Add connection state machine
- Add SYN/SYN-ACK/ACK flags to packet header
- Connection timeout handling
- Keep-alive timer (optional)

### Phase 3: Flow Control (LOW PRIORITY)
**Goal**: Prevent receiver buffer overflow.

**Features**:
1. **Sliding Window**: Limit number of un-ACKed packets in flight
2. **Window Size**: Negotiate window size during connection
3. **Backpressure**: Slow down TX when window is full

**Implementation**:
- Window size parameter (default: 10 packets)
- Track packets in flight
- Pause TX when window is full

## Quick Start: Phase 1 Implementation

### Modified Packet Structure

```c
// Enhanced packet with ACK support
typedef struct {
    uint32_t seq;           // Sequence number
    uint32_t ack_seq;       // Acknowledged sequence (0 if not ACK)
    uint8_t flags;          // ACK flag, SYN flag, FIN flag, etc.
    uint32_t t_local;       // Local timestamp
    uint8_t payload[64];    // Payload data
} __attribute__((packed)) uwb_tcp_packet_t;
```

### TX Node Changes
1. Maintain retransmission queue
2. Track ACK status for each packet
3. Retransmit on timeout
4. Remove from queue when ACKed

### RX Node Changes
1. Send ACK packet after receiving data
2. Track received sequence numbers
3. Handle duplicate packets (already received)

## Commands

### New Commands for TCP-like Mode
- `TCP_START` - Start TCP-like reliable transmission
- `TCP_SEND <data>` - Send data reliably
- `TCP_STATS` - Get TCP-like statistics

### Modified Commands
- `START` - Can work in two modes:
  - Normal mode: Fire-and-forget (current behavior)
  - TCP mode: Reliable with ACK (new behavior)

## Configuration

Add to CFG command:
```
CFG ... tcp_mode=1 ack_timeout=100 max_retries=3 window_size=10
```

## Questions for User

1. **Do you need connection management (handshake) or just reliable delivery?**
   - Option A: Just ACK + retransmission (simpler, faster)
   - Option B: Full connection management (more complex, more features)

2. **What's your priority?**
   - Reliability (ACK + retransmission) - **RECOMMENDED FIRST**
   - Connection management (handshake)
   - Flow control (sliding window)

3. **Performance vs Reliability trade-off?**
   - Fast: ACK timeout = 50ms, max retries = 2
   - Balanced: ACK timeout = 100ms, max retries = 3 (default)
   - Reliable: ACK timeout = 200ms, max retries = 5

4. **Do you need ordered delivery?**
   - Current: Packets may arrive out of order (sequence tracking only)
   - TCP-like: Reassemble out-of-order packets (requires buffer)
