UWB Test Rig Project - Technical Report

**Project:** UWB Test Orchestrator System for DWM3001C Module Testing  
**Date:** December 2024  
**Status:** Development Phase - RS-485 Communication Functional, UWB SDK Integration Pending

---

## Executive Summary

This report documents the development status of a UWB (Ultra-Wideband) test rig system designed for testing Qorvo DWM3001C modules. The project implements a dual-node test system with RS-485 command/control communication, automated test orchestration, and web-based monitoring. 

**Primary Focus:** Segger Embedded Studio implementation with Qorvo DWM SDK integration  
**Secondary Focus:** Zephyr RTOS implementation (limited by SDK compatibility)

**Current Status:**
- ✅ RS-485 command protocol fully functional
- ✅ Firmware builds and flashes successfully (both Segger and Zephyr)
- ✅ Web interface and orchestrator operational
- ⚠️ Qorvo SDK integration incomplete - UWB functionality uses stubs
- ⚠️ Zephyr implementation cannot use Qorvo SDK due to FreeRTOS dependency

---

## 1. Project Overview

### 1.1 System Architecture

The system consists of:

- **2x DWM3001CDK Development Boards** (nRF52833 MCU + DW3001 UWB transceiver)
  - Node A: TX initiator (packet source)
  - Node B: RX logger (metrics collection)

- **RS-485 Communication Infrastructure**
  - 2x USB-to-RS485 adapters (CH340)
  - 2x RS-485 to TTL converter modules
  - STP Cat 6A cable for RS-485 bus (production setup)

- **Software Components**
  - Node.js orchestrator for test automation
  - React web interface for real-time monitoring
  - Firmware for both nodes (Segger and Zephyr implementations)

### 1.2 Hardware Topology

```
PC/Raspberry Pi (Orchestrator)
    │
    ├── USB-RS485 Adapter A ──RS-485 Bus── RS-485 Module ──UART── DWM3001CDK Node A (TX)
    │
    └── USB-RS485 Adapter B ──RS-485 Bus── RS-485 Module ──UART── DWM3001CDK Node B (RX)
```

*[Image placeholder: System architecture diagram]*

### 1.3 Communication Protocol

**RS-485 Command Protocol (ASCII-based):**
- `PING` → `OK PONG` (connectivity check)
- `NODE_TYPE` → `OK NODE_TYPE=A|B` (node identification)
- `SET_CONFIG ch=5 rate=6m8 ...` → `OK` (UWB configuration)
- `START_TEST` → `OK` (begin test)
- `STOP_TEST` → `OK` (stop test)
- `GET_STATS` → `OK total_rx=X lost_pkts=Y ...` (retrieve metrics)
- `RESET_STATS` → `OK` (clear statistics)

**Baud Rate:** 115200 (Segger firmware), 38400 (Zephyr firmware)

---

## 2. Segger Embedded Studio Implementation (Primary Focus)

### 2.1 Project Structure

**Location:** `segger/uwb-test-rig/`

**Key Files:**
- `uwb-test-rig.emProject` - Segger Embedded Studio project file
- `Source/main.c` - Main application entry point
- `Source/rs485_comm.c` - RS-485 command handler
- `Source/uwb_tx.c` - Node A TX logic
- `Source/uwb_rx.c` - Node B RX logic
- `Source/uwb_config.c` - UWB configuration management
- `Source/sdk_config.h` - SDK configuration defines

**Build Output:**
- `Output/Debug/Exe/uwb-test-rig-NodeA.hex` - Node A firmware
- `Output/Debug/Exe/uwb-test-rig-NodeB.hex` - Node B firmware

### 2.2 Achievements

#### 2.2.1 RS-485 Communication

**Status:** ✅ **Fully Functional**

- UART0 configured on pins P0.08 (TXD) and P0.10 (RXD)
- Command protocol implemented and tested
- Pin pull-down issue resolved (see Section 2.3.1)
- Baud rate: 115200
- All commands respond correctly: PING, SET_CONFIG, START_TEST, STOP_TEST, GET_STATS

**Verification:**
- Commands sent from orchestrator are received by firmware
- Responses are correctly formatted and transmitted
- LED indicators on RS-485 modules confirm bidirectional communication

*[Image placeholder: RS-485 module LED activity during communication]*

#### 2.2.2 Pin Configuration Fix

**Status:** ✅ **Resolved**

**Problem:** Initial implementation did not reset GPIO pins before UART initialization, causing pins to remain in pull-down state and interfering with RS-485 communication.

**Solution:** Added pin reset code before UART initialization:
```c
nrf_gpio_cfg_default(8);   // P0.08 - TXD
nrf_gpio_cfg_default(10);  // P0.10 - RXD
```

**Result:** Segger firmware now works identically to Zephyr firmware for RS-485 communication.

#### 2.2.3 FreeRTOS Integration

**Status:** ✅ **Functional**

- FreeRTOS scheduler running
- Task management implemented
- Proper initialization sequence (SDK init before scheduler start)
- Delay functions handle pre-scheduler state correctly

#### 2.2.4 Build System

**Status:** ✅ **Operational**

- Project compiles successfully in Segger Embedded Studio
- Both Node A and Node B configurations build correctly
- Pre-built hex files available for immediate flashing
- SDK include paths documented for future integration

### 2.3 Current Limitations

#### 2.3.1 Qorvo SDK Integration Status

**Status:** ⚠️ **Incomplete - Critical Blocker**

**Current State:**
- SDK configuration structure in place (`sdk_config.h`)
- Include paths documented but not configured in project
- Code uses SDK API calls when `USE_QORVO_SDK` is defined
- **SDK not yet integrated** - firmware compiles with stubs

**What Works:**
- RS-485 command protocol (fully functional)
- Command parsing and response handling
- State machine for test control
- Statistics collection framework

**What Does Not Work:**
- Actual UWB packet transmission (uses stubs)
- Actual UWB packet reception (uses stubs)
- Real RF metrics (RSSI, SNR, etc.) - returns dummy values
- UWB MAC layer initialization

**Required Actions:**
1. Obtain Qorvo DWM3001C SDK (DW3_QM33_SDK)
2. Install SDK to: `C:/Program Files/DW3_QM33_SDK/SDK/Firmware/DW3_QM33_SDK_1.1.1/`
3. Add SDK include paths to Segger project:
   - Qorvo SDK headers
   - nRF SDK headers (bundled with Qorvo SDK)
4. Link SDK libraries
5. Complete `uwb_stack_init()` in `main.c` with actual SDK initialization
6. Test UWB packet transmission/reception

**Documentation:** See `segger/uwb-test-rig/SDK_SETUP_GUIDE.md` for detailed setup instructions.

#### 2.3.2 Code Quality Issues

**Status:** ⚠️ **Known Issues Identified**

Several code quality issues have been identified but not yet addressed:

1. **Memory Leak** (High Priority)
   - Location: `rs485_comm.c`
   - Issue: `my_strdup()` allocated memory not freed on success path
   - Impact: Memory leak on every SET_CONFIG command

2. **Race Conditions** (High Priority)
   - Location: `main.c` UART callback
   - Issue: Static variables accessed without synchronization
   - Impact: Potential data corruption if interrupts enabled

3. **Incorrect Statistics** (Medium Priority)
   - Location: `uwb_tx.c`
   - Issue: Stats incremented even when packets not actually sent (SDK stubs)
   - Impact: Misleading test results

**Reference:** See `FIRMWARE_SOURCE_ANALYSIS.md` for complete analysis.

---

## 3. Zephyr RTOS Implementation (Secondary Focus)

### 3.1 Project Structure

**Location:** `firmware/`

**Key Files:**
- `CMakeLists.txt` - Build configuration
- `prj.conf` - Zephyr configuration
- `dwm3001cdk.overlay` - Device tree overlay (UART pins)
- `src/main.c` - Main application
- `src/rs485_comm.c` - RS-485 command handler
- `src/uwb_tx.c`, `src/uwb_rx.c` - UWB TX/RX logic
- `include/sdk_config.h` - SDK configuration

### 3.2 Achievements

#### 3.2.1 RS-485 Communication

**Status:** ✅ **Fully Functional**

- Device tree properly configures UART0 pins
- Automatic pin reset before UART initialization (via device tree)
- Command protocol works identically to Segger implementation
- Baud rate: 38400 (configurable in device tree)

#### 3.2.2 Build System

**Status:** ✅ **Operational**

- Builds successfully with nRF Connect SDK
- West tool integration working
- Board support for `decawave_dwm3001cdk` configured
- Flashing via `west flash` works correctly

### 3.3 Critical Limitation: Qorvo SDK Compatibility

**Status:** ❌ **Cannot Use Qorvo SDK**

**Root Cause:**
The Qorvo DWM3001C SDK is **FreeRTOS-specific** and requires:
- FreeRTOS (FreeRTOSConfig.h)
- nRF SDK (app_util_platform.h and other nRF headers)
- FreeRTOS scheduler and task management

**Zephyr RTOS** does not use FreeRTOS, making direct SDK integration impossible without significant engineering effort.

**Current Workaround:**
- SDK integration code exists but uses stubs
- `USE_QORVO_SDK` cannot be enabled for Zephyr builds
- Firmware compiles and runs but has no actual UWB functionality

**Options:**
1. **Use Segger Implementation** (Recommended)
   - Segger uses FreeRTOS, compatible with Qorvo SDK
   - Use `segger/uwb-test-rig/` for production firmware

2. **Create Compatibility Layer** (Complex)
   - Map FreeRTOS APIs to Zephyr APIs
   - Map nRF SDK APIs to Zephyr/nRF Connect SDK APIs
   - Significant engineering effort required

3. **Disable SDK for Zephyr** (Current)
   - Keep stubs enabled
   - Use for development/testing without UWB functionality

**Reference:** See `firmware/SDK_ZEPHYR_LIMITATION.md` for detailed analysis.

---

## 4. Hardware Configuration

### 4.1 Pin Connections

**DWM3001CDK J10 Header → RS-485 TTL Module:**

```
DWM3001CDK J10          RS-485 TTL Module
──────────────          ─────────────────
Pin 2 or 4 (5V0)   ───► VCC (Power)
Pin 6 (GND)        ───► GND (Ground)
Pin 8 (P0.08)      ───► TXD (UART TX from board)
Pin 10 (P0.10)     ───► RXD (UART RX to board)
```

**Critical:** P0.08 and P0.10 are UART0 pins. Other pin combinations (P0.11/P0.06) do NOT work.

*[Image placeholder: J10 pinout diagram with RS-485 module connections]*

### 4.2 RS-485 Bus Wiring

**USB-to-RS485 Adapter → RS-485 TTL Module:**

```
USB-to-RS485        RS-485 TTL Module
Adapter             (on each board)
────────────        ─────────────────
A+ (or D+)      ───► A+ (or D+)
B- (or D-)      ───► B- (or D-)
GND             ───► GND (if available)
```

**Cable:** STP Cat 6A cable for production (50m), UTP Cat 5e acceptable for short runs

*[Image placeholder: RS-485 bus wiring diagram]*

### 4.3 Power Supply

**Options:**
1. **J-Link USB-C** (recommended for development)
   - Provides 5V via J-Link
   - No external power needed

2. **External 5V Power to J10**
   - Pin 2 or Pin 4: 5V input
   - Pin 6: GND
   - For standalone operation without J-Link

**DO NOT:** Power via GPIO pins (insufficient current capacity)

---

## 5. Software/Orchestrator System

### 5.1 Architecture

**Backend:**
- Node.js with Express.js
- RS-485 serial communication via `serialport`
- Socket.io for WebSocket updates
- CSV logging for test results

**Frontend:**
- React with Vite
- Real-time dashboard
- Historical data view
- Test configuration interface

### 5.2 Achievements

**Status:** ✅ **Fully Operational**

- Web interface accessible at `http://localhost:5000`
- API endpoints functional (REST + WebSocket)
- Real-time metrics display
- CSV logging with all required fields
- Test orchestration working
- Swagger API documentation available

**API Endpoints:**
- `GET /api/status` - Current test status
- `GET /api/stats` - Latest statistics from both nodes
- `GET /api/history` - Historical CSV data
- `POST /api/test/start` - Start test sequence
- `POST /api/test/stop` - Stop test
- `GET /api-docs` - Swagger UI

### 5.3 Configuration

**Serial Port Configuration** (`config/settings.yaml`):
```yaml
serial:
  node_a_port: COM17  # Windows: COMxx, Linux: /dev/ttyUSB0
  node_b_port: COM18  # Windows: COMxx, Linux: /dev/ttyUSB1
  baudrate: 115200    # Must match firmware (Segger: 115200, Zephyr: 38400)
  timeout: 2.0
```

**Test Plan Configuration** (`config/testPlan.json`):
- JSON array of test configurations
- Parameters: channel, data_rate, tx_power_idx, pkt_rate_hz, duration_sec

---

## 6. Key Technical Challenges and Solutions

### 6.1 UART Pin Configuration

**Problem:** Initial pin mappings (P0.11/P0.06) didn't work for UART communication.

**Root Cause:** P0.11/P0.06 are UART1 pins, but firmware was configured for UART0.

**Solution:** Switched to P0.08 (TXD) and P0.10 (RXD) - UART0 pins.

**Result:** ✅ UART communication working on both Segger and Zephyr implementations.

### 6.2 Pin Pull-Down Issue

**Problem:** Zephyr firmware worked, but Segger firmware didn't respond to RS-485 commands.

**Root Cause:** 
- Zephyr automatically resets GPIO pins before UART init (via device tree)
- Segger firmware didn't reset pins
- Pins remained in default GPIO state with pull-down enabled
- Pull-down pulled voltage to 0V, interfering with RS-485 signals

**Solution:** Added pin reset code before UART initialization in Segger firmware:
```c
nrf_gpio_cfg_default(8);   // P0.08
nrf_gpio_cfg_default(10);  // P0.10
```

**Result:** ✅ Segger firmware now works identically to Zephyr firmware.

### 6.3 COM Port Confusion

**Problem:** Multiple COM ports per board - which ones to use?

**Solution:**
- **RS-485 adapters (CH340):** Use for commands/responses (COM17/COM18)
- **J-Link CDC UART:** Use for debug output only (COM15/COM11)

**Best Practice:** Configure RS-485 ports in `config/settings.yaml`, monitor debug UART with serial terminal.

### 6.4 Baud Rate Mismatch

**Problem:** Firmware and orchestrator baud rates must match, or communication fails silently.

**Solution:** Standardized on 115200 baud for Segger firmware, documented in configuration files.

---

## 7. Testing and Verification

### 7.1 Communication Tests

**Status:** ✅ **Passing**

- PING command: Both nodes respond with `OK PONG`
- Command protocol: All commands work correctly
- Serial port verification: Ports identified and configured correctly

### 7.2 Hardware Verification

**Status:** ✅ **Verified**

- LED indicators on RS-485 modules blink during communication
- Voltage measurements confirm correct pin states
- RS-485 bus wiring verified with continuity tests

### 7.3 Firmware Verification

**Status:** ✅ **Verified**

- Debug UART output shows startup messages
- UART initialization successful (returns 0)
- Command processing working correctly

### 7.4 UWB Functionality Tests

**Status:** ❌ **Cannot Test - SDK Not Integrated**

- UWB transmission/reception cannot be tested without SDK
- Statistics return dummy values
- Actual RF performance metrics unavailable

---

## 8. What Has Been Achieved

### 8.1 Completed Components

1. ✅ **RS-485 Command Protocol**
   - Fully functional ASCII-based protocol
   - All commands implemented and tested
   - Bidirectional communication verified

2. ✅ **Firmware Build System**
   - Segger Embedded Studio project configured
   - Zephyr build system operational
   - Both implementations compile and flash successfully

3. ✅ **Hardware Configuration**
   - Pin connections verified and documented
   - RS-485 wiring tested and working
   - Power supply options identified

4. ✅ **Software Orchestrator**
   - Web interface operational
   - API endpoints functional
   - Real-time monitoring working
   - CSV logging implemented

5. ✅ **Documentation**
   - Comprehensive documentation created
   - Setup guides for both firmware implementations
   - Troubleshooting guides
   - Hardware wiring diagrams

### 8.2 Technical Achievements

1. ✅ **Pin Configuration Resolution**
   - Identified correct UART pins (P0.08/P0.10)
   - Resolved pin pull-down issue in Segger firmware
   - Verified pin states with voltage measurements

2. ✅ **FreeRTOS Integration**
   - Proper initialization sequence
   - Task management working
   - Delay functions handle pre-scheduler state

3. ✅ **Build System Configuration**
   - Segger project properly configured
   - Zephyr build system integrated
   - Pre-built hex files available

---

## 9. What Needs to Be Done

### 9.1 Critical: Qorvo SDK Integration

**Priority:** 🔴 **HIGHEST - Blocker for UWB Functionality**

**Current Status:**
- SDK integration code structure exists
- Include paths documented but not configured
- Code uses SDK API calls (when enabled)
- **SDK not yet integrated** - firmware uses stubs

**Required Actions:**

1. **Obtain Qorvo DWM3001C SDK**
   - SDK: DW3_QM33_SDK
   - Install to: `C:/Program Files/DW3_QM33_SDK/SDK/Firmware/DW3_QM33_SDK_1.1.1/`

2. **Configure Segger Project**
   - Add SDK include paths to project options
   - Add nRF SDK include paths (bundled with Qorvo SDK)
   - Link SDK libraries
   - Add preprocessor definitions

3. **Complete SDK Initialization**
   - Implement `uwb_stack_init()` in `main.c`
   - Initialize UWB MAC context
   - Register data operations
   - Test initialization sequence

4. **Verify UWB Functionality**
   - Test packet transmission (Node A)
   - Test packet reception (Node B)
   - Verify RF metrics (RSSI, SNR, etc.)
   - Test statistics reporting

**Estimated Effort:** 1-2 days  
**Documentation:** `segger/uwb-test-rig/SDK_SETUP_GUIDE.md`

### 9.2 Code Quality Improvements

**Priority:** 🟡 **MEDIUM**

**Issues Identified:**

1. **Memory Leak** (High Priority)
   - Fix memory leak in `rs485_comm.c`
   - Ensure `my_strdup()` allocated memory is freed

2. **Race Conditions** (High Priority)
   - Add synchronization for UART callback
   - Protect static variables with mutex/semaphore

3. **Incorrect Statistics** (Medium Priority)
   - Fix statistics incrementation in `uwb_tx.c`
   - Only increment when packets actually sent

**Estimated Effort:** 1 day

### 9.3 Zephyr Implementation Decision

**Priority:** 🟡 **MEDIUM**

**Current Situation:**
- Zephyr firmware works for RS-485 communication
- Cannot use Qorvo SDK (FreeRTOS dependency)
- No actual UWB functionality possible without SDK

**Options:**

1. **Abandon Zephyr for Production** (Recommended)
   - Use Segger implementation for production
   - Keep Zephyr for development/testing without UWB

2. **Create Compatibility Layer** (Complex)
   - Map FreeRTOS APIs to Zephyr APIs
   - Significant engineering effort (1-2 weeks)

3. **Maintain Both Implementations** (Current)
   - Segger for production (with SDK)
   - Zephyr for development (without SDK)

**Recommendation:** Option 1 - Use Segger for production, maintain Zephyr only for RS-485 protocol development.

### 9.4 Production Hardware Setup

**Priority:** 🟡 **MEDIUM**

**Tasks:**

1. Use STP Cat 6A cable for RS-485 bus (production)
2. Install proper cable termination (120Ω resistors)
3. Secure all connections (screw terminals, not DuPont wires)
4. Label all cables and ports
5. Create physical mounting for boards

**Estimated Effort:** 1 day

### 9.5 Enhanced Features

**Priority:** 🟢 **LOW**

**Potential Enhancements:**

1. Automated firmware flashing scripts
2. Enhanced web interface (charts, progress bars)
3. Test automation (scheduled runs, notifications)
4. Documentation consolidation

**Estimated Effort:** 2-3 days

---

## 10. Current Project Status

### 10.1 Functional Components

| Component | Status | Notes |
|-----------|--------|-------|
| RS-485 Communication | ✅ Working | Fully functional on both Segger and Zephyr |
| Command Protocol | ✅ Working | All commands implemented and tested |
| Firmware Build System | ✅ Working | Both Segger and Zephyr compile successfully |
| Web Interface | ✅ Working | Real-time monitoring operational |
| API Endpoints | ✅ Working | REST + WebSocket functional |
| CSV Logging | ✅ Working | All required fields logged |
| Pin Configuration | ✅ Working | Correct pins identified and configured |
| Hardware Wiring | ✅ Working | RS-485 bus verified |

### 10.2 Non-Functional Components

| Component | Status | Blocker |
|-----------|--------|---------|
| UWB Packet Transmission | ❌ Not Working | Qorvo SDK not integrated |
| UWB Packet Reception | ❌ Not Working | Qorvo SDK not integrated |
| Real RF Metrics | ❌ Not Working | Qorvo SDK not integrated |
| Zephyr SDK Integration | ❌ Not Possible | FreeRTOS dependency |

### 10.3 Summary

**What Works:**
- Complete RS-485 command/control infrastructure
- Firmware builds and flashes successfully
- Web-based test orchestration system
- Real-time monitoring and data logging
- Hardware configuration verified

**What Does Not Work:**
- Actual UWB packet transmission/reception (SDK not integrated)
- Real RF performance metrics (SDK stubs return dummy values)
- Zephyr cannot use Qorvo SDK (FreeRTOS dependency)

**Critical Blocker:**
- Qorvo SDK integration required for UWB functionality
- SDK must be obtained, configured, and initialized
- Estimated effort: 1-2 days once SDK is available

---

## 11. Recommendations

### 11.1 Immediate Actions

1. **Obtain Qorvo DWM3001C SDK**
   - Contact Qorvo/Decawave for SDK access
   - Install SDK to documented location
   - Verify SDK version compatibility

2. **Complete Segger SDK Integration**
   - Follow `SDK_SETUP_GUIDE.md` instructions
   - Configure include paths and libraries
   - Complete SDK initialization code
   - Test UWB functionality

3. **Fix Code Quality Issues**
   - Address memory leak in `rs485_comm.c`
   - Add synchronization for UART callback
   - Fix statistics incrementation

### 11.2 Short-Term Actions

1. **Finalize Hardware Setup**
   - Use production-grade cables and connectors
   - Install proper termination
   - Create mounting solution

2. **Decision on Zephyr Implementation**
   - Decide whether to maintain Zephyr implementation
   - If keeping, document limitations clearly
   - If abandoning, archive for reference

### 11.3 Long-Term Actions

1. **Enhanced Features**
   - Automated flashing scripts
   - Enhanced web interface
   - Test automation

2. **Documentation**
   - Consolidate troubleshooting guides
   - Create quick reference card
   - Add photos/diagrams of physical setup

---

## 12. Conclusion

The UWB Test Rig project has successfully implemented a complete RS-485 command/control infrastructure for testing DWM3001C modules. The Segger Embedded Studio firmware implementation is ready for Qorvo SDK integration, which is the critical remaining step to enable actual UWB functionality.

**Key Achievements:**
- RS-485 communication fully functional
- Firmware builds and flashes successfully
- Web-based orchestration system operational
- Hardware configuration verified and documented

**Critical Blocker:**
- Qorvo SDK integration required for UWB functionality
- SDK integration is well-documented and straightforward once SDK is available

**Next Steps:**
1. Obtain Qorvo DWM3001C SDK
2. Complete SDK integration following documented procedures
3. Test UWB packet transmission/reception
4. Verify RF metrics reporting

The project is in a strong position to complete UWB functionality once the SDK is integrated. The foundation is solid, and the remaining work is well-defined and documented.

---

## Appendix A: File References

### Key Documentation Files

- `PROJECT_SUMMARY.md` - Complete developer guide
- `segger/uwb-test-rig/SDK_SETUP_GUIDE.md` - SDK integration instructions
- `segger/FLASH_GUIDE.md` - Firmware flashing guide
- `PIN_PULLDOWN_FIX.md` - Pin configuration fix documentation
- `FIRMWARE_SOURCE_ANALYSIS.md` - Code quality analysis
- `firmware/SDK_ZEPHYR_LIMITATION.md` - Zephyr SDK compatibility analysis

### Configuration Files

- `config/settings.yaml` - Serial port and web server configuration
- `config/testPlan.json` - Test plan configuration
- `.env.win` / `.env.linux` - Platform-specific settings

### Source Code Locations

- `segger/uwb-test-rig/Source/` - Segger firmware source code
- `firmware/src/` - Zephyr firmware source code
- `src/orchestrator/` - Node.js orchestrator modules
- `frontend/src/` - React frontend components

---

## Appendix B: Image Placeholders

The following images should be added to this report:

1. **System Architecture Diagram** (Section 1.2)
   - Physical topology showing PC, USB adapters, RS-485 bus, and DWM3001CDK boards

2. **J10 Pinout Diagram** (Section 4.1)
   - DWM3001CDK J10 header pinout with RS-485 module connections labeled

3. **RS-485 Bus Wiring Diagram** (Section 4.2)
   - USB adapter to RS-485 module wiring with cable color assignments

4. **RS-485 Module LED Activity** (Section 2.2.1)
   - Photo showing LED indicators during communication

5. **Hardware Setup Photo** (Section 4)
   - Complete hardware setup showing boards, modules, and cables

---

**Report Generated:** December 2024  
**Version:** 1.0  
**Status:** Development Phase