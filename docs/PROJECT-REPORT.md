# UWB Test Rig Project - Technical Report

**Project:** UWB Test Orchestrator System for DWM3001C Module Testing  
**Date:** December 2025
**Status:** Development Phase - Communication System Working, Radio Functionality Pending

---

## Quick Summary for Management

**What is this project?**  
A test system for evaluating Ultra-Wideband (UWB) radio modules. Think of it as a quality control system that automatically tests whether radio chips work correctly.

**What works right now?**
- ✅ The control system works perfectly - we can send commands to the test boards and receive responses
- ✅ The web interface works - you can monitor tests in real-time through a browser
- ✅ The hardware is properly connected and verified
- ✅ The software that runs on the test boards compiles and can be installed successfully

**What doesn't work yet?**
- ❌ The actual radio transmission/reception (the main purpose) - this requires special software from the chip manufacturer that we're still integrating
- ⚠️ We're evaluating an alternative approach that might be faster

**Bottom Line:**  
The foundation is solid and working. We're waiting on software integration to enable the actual radio testing functionality.

---

## Glossary of Terms

**UWB (Ultra-Wideband):** A type of radio technology used for precise distance measurement and location tracking. Think of it like GPS but for indoor use.

**Firmware:** The software that runs directly on the hardware boards. Like an operating system for the test boards.

**RS-485:** A type of communication cable/standard used to send commands over long distances (up to 50 meters). Similar to how USB connects devices, but designed for industrial use.

**SDK (Software Development Kit):** Pre-written software code provided by the chip manufacturer that handles the complex radio operations. Without it, we can't make the radios work.

**Node A / Node B:** The two test boards. Node A sends radio signals, Node B receives them and measures performance.

**Orchestrator:** The main control computer that runs tests automatically and collects results.

**Segger / Zephyr:** Two different software development environments. We're primarily using Segger, which is compatible with the radio chip's required software.

**FreeRTOS:** A type of operating system for small computers. The radio chip's software requires this specific type.

**Baud Rate:** The speed at which data is sent over a cable. Think of it like the speed limit on a highway - both sides need to agree on the same speed.

**UART:** A way for the board to send and receive data through wires. Like a serial port on an old computer. The DWM3001CDK has multiple UART ports (UART0, UART1), each with specific pins assigned. You must use the correct pins for the UART port you're using.

**GPIO:** General Purpose Input/Output - pins that can be configured for different purposes. Pins can be in GPIO mode or UART mode, and switching between modes requires proper initialization.

**Pull-down / Pull-up:** Electrical resistors that weakly connect a pin to ground (pull-down) or power (pull-up). These can interfere with UART signals if not properly disabled.

**TX / RX:** Transmit (send) and Receive (get). Node A transmits signals, Node B receives them.

---

## Executive Summary

This report documents the development status of a test system designed for testing Ultra-Wideband (UWB) radio modules. The system uses two test boards connected to a control computer via long-distance cables. The control computer can send commands to the boards, run automated tests, and display results through a web browser.

**What We're Building:**
- A system that automatically tests radio modules to ensure they work correctly
- Two test boards: one sends signals, one receives and measures them
- A web interface where you can see test results in real-time
- Automated test sequences that run without manual intervention

**Current Status:**
- ✅ **Communication System:** Fully working - we can send commands and receive responses
- ✅ **Web Interface:** Operational - you can view test results in a browser
- ✅ **Hardware Setup:** Verified and working - all cables and connections tested
- ✅ **Software Compilation:** Both versions compile successfully and can be installed
- ⚠️ **Radio Functionality:** Not yet working - requires special software from chip manufacturer
- 🔄 **New Approach:** Evaluating an alternative method that might be faster to implement

**The Main Challenge:**
The radio chips need special software (called an SDK) from the manufacturer to work. We have two options:
1. Integrate the manufacturer's SDK ourselves (1-2 days of work)
2. Use a pre-built solution from another developer (2-3 days, but might be easier)

We're currently evaluating which approach will be faster and more reliable.

---

## 1. Project Overview

### 1.1 What This System Does

**In Simple Terms:**  
This is a quality control system for radio chips. It automatically tests whether the chips can send and receive signals correctly, measures how well they perform, and records the results.

**The Components:**

1. **Two Test Boards (DWM3001CDK)**
   - These are small computers with radio chips built in
   - **Node A:** Acts as the "sender" - transmits radio signals
   - **Node B:** Acts as the "receiver" - listens for signals and measures performance
   - Each board has its own small computer (microcontroller) that runs our test software

2. **Communication Cables (RS-485)**
   - Special cables that can send commands over long distances (up to 50 meters)
   - Similar to USB cables, but designed for industrial use
   - Allows the control computer to send commands to both boards simultaneously

3. **Control Computer (Orchestrator)**
   - A regular computer (PC or Raspberry Pi) that runs the test software
   - Sends commands to the boards: "start test", "change settings", "send results"
   - Collects all the test data and saves it to files

4. **Web Interface**
   - A website you can open in any browser
   - Shows test results in real-time
   - Lets you start/stop tests and see statistics

### 1.2 How Everything Connects

```
Control Computer (PC/Raspberry Pi)
    │
    ├── USB Cable → USB-to-RS485 Adapter → RS-485 Cable → Board A (Sender)
    │
    └── USB Cable → USB-to-RS485 Adapter → RS-485 Cable → Board B (Receiver)
```

**In Text:**
- The control computer connects to two USB adapters
- Each adapter converts USB signals to RS-485 signals
- RS-485 cables run to each test board (can be up to 50 meters long)
- The boards receive commands and send back test results

*[Image placeholder: System architecture diagram showing all connections]*

### 1.3 How Commands Work

The control computer sends simple text commands to the boards, like:

- **PING** → Board responds "OK PONG" (like a heartbeat - confirms it's alive)
- **START_TEST** → Board starts sending/receiving radio signals
- **STOP_TEST** → Board stops the test
- **GET_STATS** → Board sends back test results (how many signals received, etc.)
- **SET_CONFIG** → Board changes its radio settings (frequency, power, etc.)

Think of it like texting commands to the boards - they understand simple text messages and respond accordingly.

**Communication Speed:** The boards and computer talk at 115,200 characters per second (very fast, but both sides must use the same speed).

---

## 2. Software Development Status

### 2.1 What We've Built

**Location of Code:** `segger/uwb-test-rig/`

**What Each File Does:**
- `main.c` - The main program that starts everything
- `rs485_comm.c` - Handles receiving and responding to commands
- `uwb_tx.c` - Code for Node A (the sender)
- `uwb_rx.c` - Code for Node B (the receiver)
- `uwb_config.c` - Manages radio settings

**Output Files:**
- When we compile the code, we get two files:
  - `uwb-test-rig-NodeA.hex` - Software for the sender board
  - `uwb-test-rig-NodeB.hex` - Software for the receiver board
- These files are installed (flashed) onto the boards

### 2.2 What's Working

#### ✅ Communication System (Fully Functional)

**Status:** Complete and tested

**What This Means:**
- The control computer can successfully send commands to both boards
- The boards receive commands and respond correctly
- All command types work: PING, START_TEST, STOP_TEST, GET_STATS, etc.
- We can see LED lights on the communication modules blinking when data is sent (visual confirmation)

**Technical Details:**
- Communication happens through pins P0.08 (send/TXD) and P0.10 (receive/RXD) on each board
- These are the UART0 pins on the nRF52833 microcontroller
- Speed: 115,200 characters per second (baud rate)
- We fixed multiple pin configuration issues (see Section 6.1 for detailed explanation):
  - Initially tried wrong pins (UART1 instead of UART0)
  - Had to add pin reset code to remove pull-down resistors
  - Different software frameworks required different initialization approaches

**Verification:**
- ✅ Commands sent from the control computer are received by the boards
- ✅ Responses come back correctly formatted
- ✅ LED indicators confirm data is flowing both ways

#### ✅ Pin Configuration Fix

**What Was Wrong:**  
Initially, the boards weren't responding to commands. The pins used for communication were stuck in the wrong mode.

**What We Fixed:**  
We added code to properly reset the pins before starting communication. This is like making sure a door is unlocked before trying to open it.

**Result:**  
Now the boards respond to commands just like they should.

#### ✅ Software Build System

**Status:** Working

**What This Means:**
- The code compiles successfully (no errors)
- We can create software files for both Node A and Node B
- The software files are ready to be installed on the boards
- We have documentation on how to set up the radio chip software (when we get it)

### 2.3 What's Not Working Yet

#### ⚠️ Radio Functionality (Critical Blocker)

**Status:** Not Yet Working - This is the main missing piece

**What Should Work:**
- Node A should transmit actual radio signals
- Node B should receive those signals
- We should get real measurements: signal strength, distance accuracy, etc.

**What Actually Works:**
- ✅ Command system works (we can tell boards what to do)
- ✅ Boards respond to commands correctly
- ✅ Test control logic works (start/stop tests)
- ✅ Statistics collection framework is ready

**What Doesn't Work:**
- ❌ Actual radio transmission (uses placeholder code)
- ❌ Actual radio reception (uses placeholder code)
- ❌ Real signal measurements (returns fake/test values)
- ❌ Radio chip initialization

**Why It Doesn't Work:**
The radio chips need special software (SDK) from the manufacturer (Qorvo) to function. This software handles all the complex radio operations. Without it, we can only simulate what the radios would do.

**What We Need:**
1. Get the Qorvo DWM3001C SDK (Software Development Kit) from the manufacturer
2. Install it on our development computer
3. Configure our project to use it
4. Complete the initialization code
5. Test that radios actually transmit and receive

**Estimated Time:** 1-2 days once we have the SDK

#### 🔄 Alternative Approach: Pre-Built Solution

**Status:** Under Evaluation

**What We're Looking At:**
We found a pre-built solution from another developer that might be faster: [DWM3001C-starter-firmware](https://github.com/InVeritaSoft/DWM3001C-starter-firmware)

**Why This Might Be Better:**
- Already has the radio chip software integrated and working
- Includes a complete development environment (Docker container)
- Has examples of all radio functions already working
- Designed specifically for the same hardware we're using
- Includes tools for building, installing, and debugging

**What We Need to Evaluate:**
- Can we adapt it to work with our command system?
- How easy is it to integrate with our test orchestrator?
- Will it be faster than doing it ourselves?

**Next Steps:**
1. Download and examine the pre-built solution
2. Test if it builds and runs on our hardware
3. See how the radio functionality works
4. Determine if we can adapt it to our needs
5. Decide: use this or continue with our custom integration

**Estimated Time:** 2-3 days for evaluation and integration (if we choose this path)

#### ⚠️ Code Quality Issues (Known Problems)

**Status:** Issues Identified But Not Yet Fixed

**Problems Found:**

1. **Memory Leak** (High Priority)
   - **What:** The program allocates memory but doesn't always free it
   - **Impact:** Over time, the board could run out of memory
   - **When:** Happens every time a configuration command is sent
   - **Fix Needed:** Ensure memory is properly freed after use

2. **Potential Data Corruption** (High Priority)
   - **What:** Some variables could be accessed by multiple parts of the code simultaneously
   - **Impact:** Could cause incorrect data or crashes
   - **Fix Needed:** Add protection (like a lock) to prevent simultaneous access

3. **Incorrect Statistics** (Medium Priority)
   - **What:** Statistics are incremented even when packets aren't actually sent (because radio isn't working yet)
   - **Impact:** Test results would be misleading
   - **Fix Needed:** Only count statistics when packets are actually transmitted

**Estimated Fix Time:** 1 day

---

## 3. Alternative Software Approach (Secondary Option)

### 3.1 Zephyr Implementation

**What This Is:**
We also created a version using a different software development environment called "Zephyr." This was an experiment to see if it would be easier to work with.

**Status:** Partially Working

**What Works:**
- ✅ Communication system works (same as Segger version)
- ✅ Commands are received and responded to correctly
- ✅ Code compiles and can be installed on boards

**What Doesn't Work:**
- ❌ Cannot use the radio chip's required software (SDK)
- ❌ No actual radio functionality possible

**Why It Can't Work:**
The radio chip's software requires a specific type of operating system (FreeRTOS). Zephyr uses a different operating system, so they're incompatible. It would require significant engineering work (1-2 weeks) to make them work together.

**Decision:**
We're focusing on the Segger version for production use, since it's compatible with the radio chip software. The Zephyr version can be kept for testing communication protocols, but won't be used for actual radio testing.

---

## 4. Hardware Setup

### 4.1 How the Boards Connect

**DWM3001CDK Board → RS-485 Communication Module:**

```
Board Connector (J10)          RS-485 Module
───────────────────          ───────────────
Pin 2 or 4 (Power)      →     VCC (Power)
Pin 6 (Ground)          →     GND (Ground)
Pin 8 (Send Data)       →     TXD (Transmit)
Pin 10 (Receive Data)    →     RXD (Receive)
```

**Critical Pin Selection Details:**

**Why Pins 8 and 10?**
- These are the **UART0** pins on the nRF52833 microcontroller
- P0.08 = TXD (Transmit Data) - sends data from board to RS-485 module
- P0.10 = RXD (Receive Data) - receives data from RS-485 module to board
- UART0 is the primary serial communication port we're using

**Why Other Pins Don't Work:**
- **Pins P0.11 and P0.06:** These are UART1 pins, but our software uses UART0
- **Other pins:** Not configured for UART functionality
- Using wrong pins results in complete communication failure (no error messages)

**Common Mistakes to Avoid:**
- ❌ Don't use pins from UART1 when software expects UART0
- ❌ Don't assume any GPIO pin can be used for UART
- ❌ Don't forget to reset pins before UART initialization (see Section 6.1)
- ✅ Always use P0.08 (TXD) and P0.10 (RXD) for UART0 communication
- ✅ Always reset pins to default state before configuring for UART

**Pin Configuration Requirements:**
- Pins must be reset to default state before UART initialization
- This removes pull-up/pull-down resistors that interfere with signals
- Different software frameworks handle this differently (see Section 6.1 for details)

*[Image placeholder: Diagram showing board connections with pin numbers and UART labels]*

### 4.2 Communication Cable Setup

**USB Adapter → RS-485 Module:**

```
USB-to-RS485 Adapter    RS-485 Module
───────────────────    ───────────────
A+ (or D+)         →    A+ (or D+)
B- (or D-)         →    B- (or D-)
GND                →    GND
```

**Cable Type:** 
- Production: Shielded twisted pair (STP) Cat 6A cable (up to 50 meters)
- Development: Regular Cat 5e cable works for short distances

*[Image placeholder: Cable wiring diagram]*

### 4.3 Power Options

**Option 1: USB-C via J-Link (Recommended for Development)**
- Connect USB-C cable to the board's J-Link port
- Provides power automatically
- No external power supply needed

**Option 2: External 5V Power**
- Connect 5V power supply to pins 2 or 4 on connector J10
- Connect ground to pin 6
- Use when running without J-Link connection

**Important:** Don't try to power through the small GPIO pins - they can't provide enough current.

---

## 5. Control Software (Orchestrator)

### 5.1 What It Does

**Backend (Server):**
- Runs on Node.js (JavaScript runtime)
- Communicates with boards via RS-485 cables
- Sends real-time updates to web interface
- Saves test results to CSV files

**Frontend (Web Interface):**
- Built with React (web framework)
- Shows real-time test dashboard
- Displays historical test data
- Lets you configure and start tests

### 5.2 Current Status

**Status:** ✅ Fully Operational

**What Works:**
- Web interface accessible at `http://localhost:5000`
- All API endpoints functional
- Real-time metrics display
- Test results saved to CSV files
- Automated test sequences work
- API documentation available

**Available Features:**
- View current test status
- See latest statistics from both boards
- View historical test data
- Start test sequences
- Stop running tests
- Access API documentation

### 5.3 Configuration

**Serial Port Settings** (`config/settings.yaml`):
```yaml
serial:
  node_a_port: COM17  # Windows: COMxx, Linux: /dev/ttyUSB0
  node_b_port: COM18  # Windows: COMxx, Linux: /dev/ttyUSB1
  baudrate: 115200    # Must match firmware speed
  timeout: 2.0
```

**What This Means:**
- Tells the software which USB ports the boards are connected to
- Sets the communication speed (must match what the boards expect)
- Sets how long to wait for responses

**Test Plan Configuration:**
- Stored in `config/testPlan.json`
- Defines what tests to run: frequency, power level, test duration, etc.

---

## 6. Problems We Solved

### 6.1 UART Pin Configuration Challenges on DWM3001CDK

**The Problem:**  
Getting UART (serial communication) to work on the DWM3001CDK board was more difficult than expected. We encountered multiple issues that prevented communication from working.

#### Issue 1: Wrong UART Port Selection

**What Happened:**
- Initially, we tried to use pins P0.11 (for sending) and P0.06 (for receiving)
- The boards wouldn't communicate at all - no data was transmitted or received
- Commands sent from the computer were never received by the boards

**Why It Failed:**
- Pins P0.11 and P0.06 belong to **UART1** (the second UART port)
- Our software was configured to use **UART0** (the first UART port)
- It's like trying to plug a phone charger into a USB port - they're different systems that don't match

**The Solution:**
- Switched to pins P0.08 (for sending/TXD) and P0.10 (for receiving/RXD)
- These pins are the correct ones for **UART0**, which our software uses
- UART0 is the primary communication port on the nRF52833 microcontroller

**Why This Matters:**
- The DWM3001CDK board has multiple UART ports available
- Each UART port has specific pins assigned to it
- You can't mix and match pins from different UART ports
- The board's documentation doesn't always make this clear

**Result:**  
✅ Communication now works correctly with the right pins.

#### Issue 2: Pin State Configuration Problem

**What Happened:**
- After fixing the pin selection, one software version (Zephyr) worked perfectly
- The other software version (Segger) still didn't respond to commands
- Commands were sent but boards remained silent

**Why It Failed:**
- When the board starts up, pins can be in different electrical states
- By default, GPIO pins often have "pull-down" resistors enabled
- Pull-down means the pin is weakly connected to ground (0 volts)
- When we tried to use these pins for UART communication, the pull-down was interfering with the signals
- It's like trying to have a conversation while someone is constantly whispering "zero" in your ear - the signal gets corrupted

**The Technical Details:**
- **Zephyr version:** Automatically resets pins to the correct state before initializing UART (via device tree configuration)
- **Segger version:** Didn't reset pins, so they stayed in their default GPIO state with pull-down enabled
- The pull-down resistor was pulling the signal voltage down, making it impossible for the RS-485 module to read the data correctly

**The Solution:**
We added code to explicitly reset the pins before initializing UART:

```c
// Reset pins to default state (removes pull-down)
nrf_gpio_cfg_default(8);   // P0.08 - TXD (Transmit)
nrf_gpio_cfg_default(10);  // P0.10 - RXD (Receive)
```

This code tells the microcontroller: "Before we use these pins for UART, reset them to their default state and remove any pull-up or pull-down resistors."

**Why This Was Necessary:**
- Different software frameworks handle pin initialization differently
- Zephyr does it automatically, Segger requires explicit code
- Without this reset, the pins remain in GPIO mode with pull-down enabled
- The pull-down interferes with UART signals, causing communication to fail

**Result:**  
✅ Both software versions now work identically after adding the pin reset code.

#### Issue 3: Understanding the DWM3001CDK Pin Layout

**The Challenge:**
- The DWM3001CDK board has a connector called "J10" with many pins
- Not all pins can be used for UART communication
- The board documentation doesn't always clearly indicate which pins support which functions
- Some pins are shared between multiple functions (GPIO, UART, SPI, etc.)

**What We Learned:**
- **UART0 pins:** P0.08 (TXD), P0.10 (RXD) - These work for our application
- **UART1 pins:** P0.11 (TXD), P0.06 (RXD) - These don't work with our software configuration
- **Other pins:** Many pins are available but not suitable for UART

**Why P0.08 and P0.10 Are Critical:**
- These are the primary UART pins on the nRF52833 chip
- They're designed specifically for serial communication
- They have the correct electrical characteristics for RS-485 communication
- They're not shared with other critical functions on the board

**Important Note:**
- You cannot use just any pins for UART - they must be the specific pins assigned to the UART peripheral
- Using wrong pins results in complete communication failure (no error messages, just silence)
- This is a common source of confusion when working with embedded systems

### 6.2 Summary of UART/Pin Difficulties

**The Main Challenges:**

1. **Pin Selection Confusion**
   - Multiple UART ports available (UART0, UART1)
   - Each port has specific pins
   - Easy to select wrong pins
   - No clear error when wrong pins are used

2. **Pin State Management**
   - Pins can be in different modes (GPIO, UART, etc.)
   - Default state may interfere with UART
   - Different software frameworks handle this differently
   - Requires explicit code in some cases

3. **Documentation Gaps**
   - Board documentation doesn't always clearly show UART pin assignments
   - Need to refer to microcontroller datasheet
   - Trial and error often required

4. **Silent Failures**
   - Wrong pin configuration doesn't produce error messages
   - Communication simply doesn't work
   - Difficult to diagnose without oscilloscope or logic analyzer

**Lessons Learned:**
- Always verify pin assignments against the microcontroller datasheet
- Test pin states with a multimeter or oscilloscope if communication fails
- Reset pins to default state before configuring them for UART
- Don't assume all software frameworks handle pin initialization the same way
- When in doubt, check the actual electrical signals with test equipment

**Result:**  
✅ All issues resolved - communication now works reliably on both software versions.

### 6.3 Multiple Communication Ports Confusion

**Problem:**  
Each board shows up as multiple communication ports - which one to use?

**Solution:**
- **RS-485 adapters:** Use for commands and responses (the ones we need)
- **J-Link debug port:** Use only for viewing debug messages

**Best Practice:** Configure the RS-485 ports in settings, use debug port only for troubleshooting.

### 6.4 Communication Speed Mismatch

**Problem:**  
If the computer and boards use different communication speeds, nothing works (and there's no error message).

**Solution:**  
Standardized on 115,200 characters per second for our main software version, documented in all configuration files.

---

## 7. Testing and Verification

### 7.1 Communication Tests

**Status:** ✅ All Passing

- PING command: Both boards respond correctly
- All command types work as expected
- Communication ports identified and configured correctly

### 7.2 Hardware Verification

**Status:** ✅ Verified

- LED lights on communication modules blink during data transfer (visual confirmation)
- Voltage measurements confirm pins are in correct states
- Cable connections verified with continuity tests

### 7.3 Software Verification

**Status:** ✅ Verified

- Debug output shows startup messages
- Communication initialization successful
- Command processing works correctly

### 7.4 Radio Functionality Tests

**Status:** ❌ Cannot Test Yet

**Why:**
- Radio transmission/reception requires the manufacturer's SDK
- Without it, we can only test the command system
- Statistics return placeholder values, not real measurements

---

## 8. What We've Accomplished

### 8.1 Completed Components

1. ✅ **Communication System**
   - Fully functional command protocol
   - All commands implemented and tested
   - Two-way communication verified

2. ✅ **Software Build System**
   - Both software versions compile successfully
   - Software files can be created and installed on boards
   - Build process is documented

3. ✅ **Hardware Configuration**
   - All connections verified and documented
   - Communication cables tested and working
   - Power supply options identified

4. ✅ **Control Software**
   - Web interface operational
   - All features functional
   - Real-time monitoring working
   - Test results logging implemented

5. ✅ **Documentation**
   - Comprehensive guides created
   - Setup instructions for both software versions
   - Troubleshooting guides
   - Hardware wiring diagrams

### 8.2 Technical Achievements

1. ✅ **Pin Configuration**
   - Identified correct communication pins
   - Fixed pin state issues
   - Verified with voltage measurements

2. ✅ **Operating System Integration**
   - Proper startup sequence
   - Task management working
   - Timing functions handle all states correctly

3. ✅ **Build System**
   - Software projects properly configured
   - Both versions build successfully
   - Ready-to-install files available

---

## 9. What Still Needs to Be Done

### 9.1 Critical: Enable Radio Functionality

**Priority:** 🔴 **HIGHEST - This is the main blocker**

**Current Situation:**
- Command system works perfectly
- Code structure is ready for radio functionality
- **Radio functionality not working** - needs manufacturer's software

**Two Approaches We're Evaluating:**

#### Approach A: Custom Integration (Original Plan)

**What We Need:**
1. Get Qorvo DWM3001C SDK from manufacturer
2. Install it on development computer
3. Configure our project to use it
4. Complete the initialization code
5. Test radio transmission and reception

**Estimated Time:** 1-2 days  
**Documentation:** Setup guide already prepared

#### Approach B: Pre-Built Solution (New Option)

**What We're Evaluating:**
- Use [DWM3001C-starter-firmware](https://github.com/InVeritaSoft/DWM3001C-starter-firmware) repository
- Already has radio functionality working
- Includes complete development environment
- Designed for the same hardware

**What We Need to Do:**
1. Download and examine the repository
2. Test if it builds and runs
3. Assess radio functionality
4. Determine integration path with our command system
5. Decide whether to adopt this approach

**Estimated Time:** 2-3 days (evaluation + integration)

**Decision:** Currently evaluating both approaches to determine the fastest path forward.

### 9.2 Code Quality Improvements

**Priority:** 🟡 **MEDIUM**

**Issues to Fix:**

1. **Memory Leak** (High Priority)
   - Fix: Ensure all allocated memory is properly freed
   - Impact: Prevents memory exhaustion over time

2. **Data Protection** (High Priority)
   - Fix: Add protection for shared variables
   - Impact: Prevents data corruption and crashes

3. **Statistics Accuracy** (Medium Priority)
   - Fix: Only count statistics when packets are actually sent
   - Impact: Ensures test results are accurate

**Estimated Time:** 1 day

### 9.3 Software Version Decision

**Priority:** 🟡 **MEDIUM**

**Current Situation:**
- We have two software versions (Segger and Zephyr)
- Segger version can use radio chip software
- Zephyr version cannot use radio chip software

**Recommendation:**
- Use Segger version for production (it works with radio chips)
- Keep Zephyr version for testing communication only
- Don't invest more time in Zephyr for radio functionality

### 9.4 Production Hardware Setup

**Priority:** 🟡 **MEDIUM**

**Tasks:**
1. Use production-grade cables (shielded Cat 6A)
2. Install proper cable terminators (120Ω resistors at ends)
3. Secure all connections (use screw terminals, not temporary wires)
4. Label all cables and ports clearly
5. Create proper mounting for boards

**Estimated Time:** 1 day

### 9.5 Future Enhancements

**Priority:** 🟢 **LOW**

**Potential Improvements:**
1. Automated software installation scripts
2. Enhanced web interface (charts, progress indicators)
3. Scheduled test automation
4. Better documentation organization

**Estimated Time:** 2-3 days

---

## 10. Current Project Status Summary

### 10.1 What's Working

| Component | Status | What This Means |
|-----------|--------|-----------------|
| Communication System | ✅ Working | Can send commands and get responses |
| Command Protocol | ✅ Working | All command types work correctly |
| Software Build | ✅ Working | Code compiles and can be installed |
| Web Interface | ✅ Working | Can view tests in browser |
| API Endpoints | ✅ Working | All features accessible |
| Data Logging | ✅ Working | Test results saved to files |
| Pin Configuration | ✅ Working | Hardware connections correct |
| Cable Wiring | ✅ Working | All cables tested and verified |

### 10.2 What's Not Working

| Component | Status | Why It's Blocked |
|-----------|--------|------------------|
| Radio Transmission | ❌ Not Working | Needs manufacturer's SDK |
| Radio Reception | ❌ Not Working | Needs manufacturer's SDK |
| Real Measurements | ❌ Not Working | Needs manufacturer's SDK |
| Zephyr Radio Support | ❌ Not Possible | Incompatible operating systems |

### 10.3 Overall Summary

**What Works:**
- Complete command and control system
- Software builds and installs successfully
- Web-based test management system
- Real-time monitoring and data logging
- All hardware verified and working

**What Doesn't Work:**
- Actual radio transmission/reception (needs SDK integration)
- Real performance measurements (returns placeholder values)
- Zephyr version cannot support radio functionality

**The Blocker:**
- Radio functionality requires manufacturer's SDK
- We're evaluating two approaches: custom integration vs. pre-built solution
- Estimated 1-3 days once we complete evaluation and choose approach

---

## 11. Recommendations

### 11.1 Immediate Actions (Next Steps)

1. **Evaluate Pre-Built Solution** (NEW)
   - Download and review the DWM3001C-starter-firmware repository
   - Test if it builds and runs on our hardware
   - Assess radio functionality
   - Determine how to integrate with our command system
   - Make decision: use this or continue with custom integration

2. **Complete Radio Integration** (Path depends on decision above)
   - **If using pre-built solution:** Follow its setup instructions and adapt to our system
   - **If continuing custom:** Get SDK from manufacturer, follow our setup guide, complete integration

3. **Fix Code Quality Issues**
   - Fix memory leak
   - Add data protection
   - Fix statistics accuracy

### 11.2 Short-Term Actions (This Month)

1. **Finalize Hardware**
   - Use production-grade cables
   - Install proper terminators
   - Create mounting solution

2. **Software Version Decision**
   - Confirm using Segger version for production
   - Document Zephyr limitations clearly

### 11.3 Long-Term Actions (Future)

1. **Enhanced Features**
   - Automated installation scripts
   - Improved web interface
   - Test scheduling automation

2. **Documentation**
   - Consolidate all guides
   - Create quick reference
   - Add photos of physical setup

---

## 12. Conclusion

The UWB Test Rig project has successfully built a complete command and control system for testing radio modules. The communication infrastructure works perfectly, the web interface is operational, and all hardware is verified. The system is ready for radio functionality integration, which is the final critical step.

**Key Achievements:**
- Communication system fully functional
- Software builds and installs successfully
- Web-based test management operational
- Hardware configuration verified and documented

**Critical Next Step:**
- Enable radio functionality through SDK integration
- Currently evaluating two approaches to determine fastest path
- Estimated 1-3 days of work once approach is chosen

**Project Status:**
The foundation is solid and working. We're in the final phase of integrating the radio functionality. Once complete, the system will be able to perform full automated testing of UWB radio modules.

---

## Appendix A: File References

### Key Documentation Files

- `PROJECT_SUMMARY.md` - Complete developer guide
- `segger/uwb-test-rig/SDK_SETUP_GUIDE.md` - SDK integration instructions
- `segger/FLASH_GUIDE.md` - Software installation guide
- `PIN_PULLDOWN_FIX.md` - Pin configuration fix documentation
- `FIRMWARE_SOURCE_ANALYSIS.md` - Code quality analysis
- `firmware/SDK_ZEPHYR_LIMITATION.md` - Zephyr compatibility analysis

### Configuration Files

- `config/settings.yaml` - Serial port and web server settings
- `config/testPlan.json` - Test plan configuration
- `.env.win` / `.env.linux` - Platform-specific settings

### Source Code Locations

- `segger/uwb-test-rig/Source/` - Main software source code
- `firmware/src/` - Alternative software version source code
- `src/orchestrator/` - Control computer software
- `frontend/src/` - Web interface code

---

## Appendix C: UART and Pin Configuration Quick Reference

### DWM3001CDK UART Pin Configuration

**Correct Configuration (UART0):**
- **TXD (Transmit):** Pin P0.08 on J10 connector
- **RXD (Receive):** Pin P0.10 on J10 connector
- **UART Port:** UART0 (primary serial port)
- **Baud Rate:** 115,200 (for Segger firmware)

**Common Mistakes:**
- ❌ Using P0.11/P0.06 (these are UART1 pins, won't work with UART0 software)
- ❌ Not resetting pins before UART initialization (causes pull-down interference)
- ❌ Assuming any GPIO pin can be used for UART (only specific pins work)

### Pin Initialization Requirements

**For Segger/FreeRTOS Implementation:**
```c
// MUST reset pins before UART initialization
nrf_gpio_cfg_default(8);   // P0.08 - TXD
nrf_gpio_cfg_default(10);  // P0.10 - RXD
// Then initialize UART...
```

**For Zephyr Implementation:**
- Pin reset handled automatically via device tree
- No explicit reset code needed
- Device tree configuration in `dwm3001cdk.overlay`

### Troubleshooting UART Communication

**If communication doesn't work, check:**

1. **Pin Selection:**
   - Verify using P0.08 (TXD) and P0.10 (RXD)
   - Confirm software is configured for UART0, not UART1

2. **Pin State:**
   - Check if pins are reset before UART init (Segger only)
   - Verify no pull-up/pull-down resistors interfering
   - Measure pin voltage with multimeter (should be ~3.3V when idle)

3. **Baud Rate:**
   - Must match between firmware and orchestrator
   - Segger: 115,200
   - Zephyr: 38,400 (configurable)

4. **Hardware Connections:**
   - Verify RS-485 module connected correctly
   - Check cable continuity
   - Confirm power is applied to RS-485 module

5. **Software Configuration:**
   - Verify UART0 is initialized (not UART1)
   - Check that pins are configured for UART mode (not GPIO)
   - Confirm interrupt handlers are set up correctly

### Why These Issues Occur

**Multiple UART Ports:**
- nRF52833 has multiple UART peripherals (UART0, UART1, etc.)
- Each has different pin assignments
- Software must match the UART port to the pins used

**Pin Multiplexing:**
- Pins can serve multiple functions (GPIO, UART, SPI, etc.)
- Must explicitly configure pin for UART function
- Default state is often GPIO with pull-down enabled

**Framework Differences:**
- Zephyr: Automatic pin configuration via device tree
- Segger: Manual pin configuration required
- Both approaches work, but require different code

**Silent Failures:**
- Wrong pin configuration doesn't generate error messages
- Communication simply doesn't work
- Requires hardware debugging (oscilloscope, logic analyzer) to diagnose

---

## Appendix B: Image Placeholders

The following images should be added to this report:

1. **Communication Module Photo** (Section 2.2)
   - Shows LED indicators during communication

2. **Complete Setup Photo** (Section 4)
   - Shows entire hardware setup

---

**Report Generated:** December 2025 
**Version:** 1.0  
**Status:** Development Phase - Ready for Radio Integration
