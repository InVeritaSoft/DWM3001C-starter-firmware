/*
 * RS485 Bridge Firmware for Arduino Uno
 * 
 * Purpose: Acts as a transparent communication bridge between RS485 orchestrator
 *          and DWM3001CDK UWB module
 * 
 * Architecture:
 *   Orchestrator <--RS485--> MAX485 <--TTL--> Arduino <--TTL--> DWM3001CDK
 * 
 * Pin Assignments:
 *   RS485 (MAX485):
 *     - D2  = RE (Receiver Enable, active LOW)
 *     - D3  = DE (Driver Enable, active HIGH)
 *     - D10 = RO (Receiver Output, reads from MAX485)
 *     - D11 = DI (Data Input, writes to MAX485)
 *   
 *   DWM3001CDK:
 *     - D8  = RX (from DWM3001CDK GPIO14/TXD0)
 *     - D9  = TX (to DWM3001CDK GPIO15/RXD0)
 *   
 *   LED Indicators:
 *     - D4  = RX Activity (blinks when receiving from orchestrator)
 *     - D5  = TX Activity (blinks when transmitting to orchestrator)
 *     - D6  = Error LED (solid during errors)
 *     - D13 = Heartbeat (1Hz blink to show Arduino is alive)
 * 
 * Supported Commands (transparent pass-through):
 *   - PNG/PING         : Connectivity check
 *   - NT/NODE_TYPE     : Query node type (TX_V2 or RX_V2)
 *   - STRT/START       : Start UWB test
 *   - STOP             : Stop UWB test
 *   - STAT/GET_STATS   : Get statistics
 *   - CFG/SET_CONFIG   : Configure UWB parameters
 * 
 * Communication Protocol:
 *   - Baud Rate: 115200
 *   - Command terminator: \r or \n
 *   - Response format: OK <data>\r\n or ERR_<type>\r\n
 * 
 * Author: Generated for INVERITA DWM3001C Test Rig
 * Version: 1.0
 * Date: 2026-01-20
 */

#include <SoftwareSerial.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// RS485 Communication Pins (MAX485 Transceiver)
#define RS485_DE_PIN 3        // Driver Enable (HIGH = transmit, LOW = receive)
#define RS485_RE_PIN 2        // Receiver Enable (LOW = receive, HIGH = transmit)
#define RS485_RO_PIN 10       // Receiver Output (SoftwareSerial RX)
#define RS485_DI_PIN 11       // Driver Input (SoftwareSerial TX)

// DWM3001CDK Communication Pins
// UPDATED: DWM firmware now uses GPIO 27 for TX and GPIO 15 for RX (to avoid LED conflict)
// - DWM TX = GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX)
// - DWM RX = GPIO 15 (P0.15) - J10 Pin 10 or Pin 15
// Arduino connections remain the same (D8/D9), but verify wiring matches DWM pins
#define DWM_RX_PIN 8          // Arduino RX from DWM TX (GPIO27, J10 Pin 19, GPIO27_PIN19_TX)
#define DWM_TX_PIN 9          // Arduino TX to DWM RX (GPIO15, J10 Pin 10/15)

// LED Indicator Pins
#define LED_RX_PIN 4          // RX Activity LED
#define LED_TX_PIN 5          // TX Activity LED
#define LED_ERROR_PIN 6       // Error LED
#define LED_HEARTBEAT_PIN 13  // Heartbeat LED (built-in)

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

#define RS485_BAUD_RATE 57600         // RS485 baud rate (reduced for SoftwareSerial reliability)
#define DWM_BAUD_RATE 115200          // DWM3001CDK baud rate (must match firmware)
#define RS485_TX_DELAY_US 100         // Delay for RS485 transceiver switching (microseconds)
#define COMMAND_BUFFER_SIZE 256       // Increased for long STAT responses
#define DWM_RESPONSE_TIMEOUT_MS 5000  // Timeout for DWM response (5 seconds)
#define HANDSHAKE_RETRY_COUNT 3       // Number of handshake retries
#define HEARTBEAT_INTERVAL_MS 500     // Heartbeat LED blink interval
#define LED_BLINK_DURATION_MS 50      // Activity LED blink duration

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Serial port instances
SoftwareSerial RS485Serial(RS485_RO_PIN, RS485_DI_PIN); // RX, TX
SoftwareSerial DWMSerial(DWM_RX_PIN, DWM_TX_PIN);       // RX, TX

// Communication buffers
char commandBuffer[COMMAND_BUFFER_SIZE];
char responseBuffer[COMMAND_BUFFER_SIZE];

// State variables
bool handshakeComplete = false;
char nodeType[16] = "UNKNOWN";  // Will be "TX_V2", "RX_V2", or "UNKNOWN"
unsigned long lastHeartbeatTime = 0;
bool heartbeatState = false;
unsigned long lastStatusTime = 0;
unsigned long lastRS485CheckTime = 0;
int rs485BytesReceived = 0;
int rs485BytesDropped = 0;

// Test mode variables
unsigned long lastTestTime = 0;
bool testModeEnabled = false;
int testCommandIndex = 0;
const char* testCommands[] = {"PNG", "NODE_TYPE", "STAT"};
const int testCommandCount = 3;


// CRITICAL: Flag to prevent listener switching during DWM communication
bool waitingForDWMResponse = false;

// State recovery variables
unsigned long lastStateCheckTime = 0;
unsigned long lastCommandTime = 0;
unsigned long lastTransmitTime = 0;  // Track when we last transmitted on RS485
#define STATE_CHECK_INTERVAL_MS 5000  // Check state every 5 seconds
#define COMMAND_TIMEOUT_MS 10000      // If no command for 10 seconds, reset state

// LED state tracking
unsigned long ledRxOffTime = 0;
unsigned long ledTxOffTime = 0;
bool ledRxActive = false;
bool ledTxActive = false;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void setup();
void loop();
void set_rs485_tx_mode();
void set_rs485_rx_mode();
bool rs485_receive_command(char* buffer, int maxLen);
void rs485_send_response(const char* response);
void dwm_send_command(const char* command);
bool dwm_receive_response(char* buffer, int maxLen);
bool perform_handshake();
void send_error(const char* errorType);
void blink_rx_led();
void blink_tx_led();
void set_error_led(bool state);
void update_heartbeat();
void update_activity_leds();
void test_dwm_communication();
bool serial_receive_command(char* buffer, int maxLen);
void handle_serial_command(const char* command);
void recover_state();

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  // Initialize Serial for debugging (USB serial port)
  Serial.begin(115200);
  
  // Wait for serial port to stabilize and clear any garbage data
  // This prevents garbled characters at startup
  // NOTE: On Arduino Uno, Serial is always available, so this won't block
  // On Leonardo/Micro, it waits for USB connection (max 3 seconds)
  unsigned long serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < 3000)) {
    ; // Wait for serial port to connect (needed for native USB boards)
  }
  delay(100);  // Additional stabilization delay
  Serial.flush();  // Clear any pending data
  
  Serial.println(F("\n=== RS485 Bridge Firmware Starting ==="));
  
  // Initialize LED pins as outputs
  pinMode(LED_RX_PIN, OUTPUT);
  pinMode(LED_TX_PIN, OUTPUT);
  pinMode(LED_ERROR_PIN, OUTPUT);
  pinMode(LED_HEARTBEAT_PIN, OUTPUT);
  Serial.println(F("[SETUP] LED pins initialized"));
  
  // Initialize all LEDs to OFF
  digitalWrite(LED_RX_PIN, LOW);
  digitalWrite(LED_TX_PIN, LOW);
  digitalWrite(LED_ERROR_PIN, LOW);
  digitalWrite(LED_HEARTBEAT_PIN, LOW);
  
  // Initialize RS485 control pins (but don't set mode yet - serial not initialized)
  pinMode(RS485_DE_PIN, OUTPUT);
  pinMode(RS485_RE_PIN, OUTPUT);
  Serial.println(F("[SETUP] RS485 control pins initialized"));
  
  // Initialize serial ports FIRST (before setting RS485 mode)
  RS485Serial.begin(RS485_BAUD_RATE);
  DWMSerial.begin(DWM_BAUD_RATE);
  Serial.print(F("[SETUP] RS485 serial initialized at "));
  Serial.print(RS485_BAUD_RATE);
  Serial.print(F(" baud, DWM serial at "));
  Serial.print(DWM_BAUD_RATE);
  Serial.println(F(" baud"));
  
  // Now set RS485 to receive mode (safe to flush now)
  set_rs485_rx_mode();
  Serial.println(F("[SETUP] RS485 set to RX mode"));
  
  // Verify RS485 pin states
  Serial.print(F("[SETUP] RS485 DE pin (should be LOW): "));
  Serial.println(digitalRead(RS485_DE_PIN) ? "HIGH" : "LOW");
  Serial.print(F("[SETUP] RS485 RE pin (should be LOW): "));
  Serial.println(digitalRead(RS485_RE_PIN) ? "HIGH" : "LOW");
  
  // CRITICAL: Power Check - MAX485 needs 5V power!
  Serial.println(F("[SETUP] =========================================="));
  Serial.println(F("[SETUP] POWER CHECK - MAX485 Module"));
  Serial.println(F("[SETUP] =========================================="));
  Serial.println(F("[SETUP] MAX485 Module MUST be powered:"));
  Serial.println(F("[SETUP]   VCC → Arduino 5V pin"));
  Serial.println(F("[SETUP]   GND → Arduino GND pin"));
  Serial.println(F("[SETUP]"));
  Serial.println(F("[SETUP] If commands don't reach Arduino:"));
  Serial.println(F("[SETUP]   1. Check MAX485 VCC is connected to Arduino 5V"));
  Serial.println(F("[SETUP]   2. Check MAX485 GND is connected to Arduino GND"));
  Serial.println(F("[SETUP]   3. Measure MAX485 VCC-GND voltage = 5.0V"));
  Serial.println(F("[SETUP]   4. Verify Arduino 5V pin outputs 5.0V"));
  Serial.println(F("[SETUP] =========================================="));
  
  // Test RS485 serial port
  Serial.print(F("[SETUP] RS485 serial port status: "));
  Serial.print(F("Listening="));
  Serial.print(RS485Serial.isListening() ? "YES" : "NO");
  Serial.print(F(", Available="));
  Serial.println(RS485Serial.available());
  
  // Small delay for serial ports to stabilize
  delay(100);
  
  // Clear any garbage data from RS485
  int cleared = 0;
  while (RS485Serial.available()) {
    RS485Serial.read();
    cleared++;
  }
  if (cleared > 0) {
    Serial.print(F("[SETUP] Cleared "));
    Serial.print(cleared);
    Serial.println(F(" bytes from RS485 buffer"));
  }
  
  // Blink all LEDs to indicate startup
  Serial.println(F("[SETUP] Startup LED sequence"));
  digitalWrite(LED_RX_PIN, HIGH);
  digitalWrite(LED_TX_PIN, HIGH);
  digitalWrite(LED_ERROR_PIN, HIGH);
  digitalWrite(LED_HEARTBEAT_PIN, HIGH);
  delay(500);
  digitalWrite(LED_RX_PIN, LOW);
  digitalWrite(LED_TX_PIN, LOW);
  digitalWrite(LED_ERROR_PIN, LOW);
  digitalWrite(LED_HEARTBEAT_PIN, LOW);
  delay(200);
  
  // CRITICAL: Check if DWM3001CDK is sending data BEFORE handshake
  // DWM3001CDK sends startup messages immediately after boot
  Serial.println(F("[SETUP] =========================================="));
  Serial.println(F("[SETUP] DWM3001CDK Communication Check"));
  Serial.println(F("[SETUP] =========================================="));
  Serial.println(F("[SETUP] Checking for DWM3001CDK startup messages..."));
  Serial.println(F("[SETUP] DWM3001CDK should send: OK STARTUP, OK FIRMWARE_RUNNING"));
  
  DWMSerial.listen();  // Switch to DWM serial
  delay(100);  // Small delay for listener switch
  
  int startupBytes = 0;
  char startupBuffer[256] = {0};
  int startupIndex = 0;
  unsigned long checkStart = millis();
  
  Serial.println(F("[SETUP] Listening for 3 seconds..."));
  while (millis() - checkStart < 3000) {  // Check for 3 seconds
    // CRITICAL FIX: Ensure listener is on DWM during startup check
    if (!DWMSerial.isListening()) {
      DWMSerial.listen();
      delay(10);
    }
    
    if (DWMSerial.available()) {
      char c = DWMSerial.read();
      startupBytes++;
      
      // Store in buffer for analysis
      if (startupIndex < sizeof(startupBuffer) - 1) {
        startupBuffer[startupIndex++] = c;
      }
      
      // Print character directly (for immediate feedback)
      if (c >= 32 && c < 127) {
        Serial.print(c);
      } else if (c == '\r') {
        Serial.print(F("\\r"));
      } else if (c == '\n') {
        Serial.print(F("\\n"));
        Serial.println();
      } else {
        Serial.print(F("."));
      }
      
      if (startupBytes > 500) break;  // Prevent buffer overflow
    } else {
      // Small delay to allow SoftwareSerial interrupt handlers to run
      delay(1);
    }
  }
  
  Serial.println();
  Serial.println(F("[SETUP] =========================================="));
  
  if (startupBytes > 0) {
    startupBuffer[startupIndex] = '\0';  // Null terminate
    Serial.print(F("[SETUP] ✅ Received "));
    Serial.print(startupBytes);
    Serial.println(F(" bytes from DWM3001CDK"));
    Serial.print(F("[SETUP] Data: \""));
    Serial.print(startupBuffer);
    Serial.println(F("\""));
    
    // Check if we got expected startup messages
    String startupStr = String(startupBuffer);
    if (startupStr.indexOf("OK STARTUP") >= 0 || startupStr.indexOf("OK FIRMWARE") >= 0) {
      Serial.println(F("[SETUP] ✅ DWM3001CDK firmware is running!"));
    } else {
      Serial.println(F("[SETUP] ⚠️  Received data but not expected startup messages"));
      Serial.println(F("[SETUP]    DWM3001CDK might be in wrong state or wrong firmware"));
    }
  } else {
    Serial.println(F("[SETUP] ❌ WARNING: No startup messages received from DWM3001CDK"));
    Serial.println(F("[SETUP] =========================================="));
    Serial.println(F("[SETUP] Troubleshooting:"));
    Serial.println(F("[SETUP]   1. DWM3001CDK powered on? (check power LED)"));
    Serial.println(F("[SETUP]   2. Wiring correct?"));
    Serial.println(F("[SETUP]      - DWM GPIO14 (RX) → Arduino D9 (TX)"));
    Serial.println(F("[SETUP]      - DWM GPIO15 (TX) → Arduino D8 (RX)"));
    Serial.println(F("[SETUP]      - DWM GND → Arduino GND"));
    Serial.println(F("[SETUP]   3. DWM3001CDK firmware flashed?"));
    Serial.println(F("[SETUP]      - Should be orchestrator_rx.c or orchestrator_tx.c"));
    Serial.println(F("[SETUP]      - Check with: .\\build-and-flash-rx.ps1 or .\\build-and-flash-tx.ps1"));
    Serial.println(F("[SETUP]   4. Baud rate match?"));
    Serial.println(F("[SETUP]      - Arduino expects: 115200 baud"));
    Serial.println(F("[SETUP]      - DWM3001CDK should use: 115200 baud"));
    Serial.println(F("[SETUP]   5. DWM3001CDK UART enabled?"));
    Serial.println(F("[SETUP]      - Firmware should initialize UART on GPIO14/15"));
    Serial.println(F("[SETUP] =========================================="));
  }
  
  // Clear any remaining data
  cleared = 0;  // Reuse variable declared earlier
  while (DWMSerial.available()) {
    DWMSerial.read();
    cleared++;
  }
  if (cleared > 0) {
    Serial.print(F("[SETUP] Cleared "));
    Serial.print(cleared);
    Serial.println(F(" additional bytes from DWM buffer"));
  }
  
  // Perform handshake with DWM3001CDK
  Serial.println(F("[SETUP] Starting handshake with DWM3001CDK..."));
  handshakeComplete = perform_handshake();
  
  if (handshakeComplete) {
    Serial.print(F("[SETUP] Handshake SUCCESS - Node Type: "));
    Serial.println(nodeType);
    // Success - blink heartbeat LED 3 times rapidly
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_HEARTBEAT_PIN, HIGH);
      delay(100);
      digitalWrite(LED_HEARTBEAT_PIN, LOW);
      delay(100);
    }
  } else {
    Serial.println(F("[SETUP] Handshake FAILED - Error LED active"));
    // Handshake failed - blink error LED rapidly
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_ERROR_PIN, HIGH);
      delay(50);
      digitalWrite(LED_ERROR_PIN, LOW);
      delay(50);
    }
    // Keep error LED on
    set_error_led(true);
  }
  
  // Initialize heartbeat timer
  lastHeartbeatTime = millis();
  lastStatusTime = millis();
  lastRS485CheckTime = millis();
  
  Serial.println(F("[SETUP] Initialization complete - entering main loop"));
  Serial.println(F("=========================================="));
  Serial.println(F("[INFO] Waiting for commands from RS485 orchestrator..."));
  Serial.println(F("[INFO] If no commands appear, check:"));
  Serial.println(F("  1. RS485 wiring (A/B, GND, VCC)"));
  Serial.println(F("  2. MAX485 DE/RE pins connected to D2/D3"));
  Serial.println(F("  3. RS485 baud rate matches (57600)"));
  Serial.println(F("  4. RS485 transceiver power"));
  Serial.println(F("  5. Orchestrator is sending commands"));
  Serial.println(F("=========================================="));
  Serial.println(F("[INFO] Serial Monitor Commands:"));
  Serial.println(F("  - Send any command (e.g., PNG, NODE_TYPE, STAT)"));
  Serial.println(F("    to forward it directly to DWM3001CDK for debugging"));
  Serial.println(F("  - Send 'T' to toggle test mode"));
  Serial.println(F("  - Commands should end with \\r or \\n"));
  Serial.println(F("==========================================\n"));
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // STATE RECOVERY: Periodically check and fix stuck states
  unsigned long currentTime = millis();
  if (currentTime - lastStateCheckTime > STATE_CHECK_INTERVAL_MS) {
    lastStateCheckTime = currentTime;
    recover_state();
  }
  
  // PRIORITY 1: Check for commands from RS485 converter (HIGHEST PRIORITY)
  // This must be checked first to ensure orchestrator commands are processed immediately
  // The actual RS485 command handling is below after status checks
  
  
  // CRITICAL FIX: Always ensure RS485 listener is active when not actively waiting for DWM
  // Commands can arrive at any time, so RS485 must always be ready
  // If waiting for DWM but RS485 has data, we'll handle it in the command processing section
  if (!RS485Serial.isListening()) {
    // Only switch if we're not in the middle of a critical DWM operation
    // But if RS485 has data, we need to switch to process it
    if (!waitingForDWMResponse || RS485Serial.available()) {
      RS485Serial.listen();
      if (waitingForDWMResponse && RS485Serial.available()) {
        Serial.println(F("[RS485] New command detected - switching to RS485 (will cancel DWM wait)"));
      } else {
        Serial.println(F("[RS485] Switched listener to RS485 port"));
      }
    }
  }
  
  // CRITICAL FIX: Force RS485 to RX mode periodically to prevent stuck TX state
  // If RS485 is stuck in TX mode, it can't receive commands
  // Commands must be receivable at ANY time, so RS485 should always be in RX mode when idle
  static unsigned long lastRS485ModeCheck = 0;
  if (currentTime - lastRS485ModeCheck > 1000) {  // Check every second
    lastRS485ModeCheck = currentTime;
    // Always check RS485 mode - it must be in RX mode to receive commands
    // Only exception: when actively transmitting (which is very brief)
    // Check if we're currently transmitting by checking if we just sent something
    unsigned long timeSinceTransmit = currentTime - lastTransmitTime;
    
    // If it's been more than 100ms since last transmit, we should be in RX mode
    if (timeSinceTransmit > 100) {
      // Verify RS485 is in RX mode (should be LOW for both DE and RE)
      bool deState = digitalRead(RS485_DE_PIN);
      bool reState = digitalRead(RS485_RE_PIN);
      if (deState == HIGH || reState == HIGH) {
        // RS485 is stuck in TX mode - force it back to RX mode
        // This is critical - commands can't be received in TX mode!
        Serial.println(F("[STATE] RS485 stuck in TX mode - forcing RX mode (commands must be receivable)"));
        set_rs485_rx_mode();
        delay(10);  // Allow mode switch to complete
      }
    }
  }
  
  // Update heartbeat LED
  update_heartbeat();
  
  // Update activity LEDs (turn off after blink duration)
  update_activity_leds();
  
  // Periodic status message every 10 seconds
  unsigned long currentTime = millis();
  if (currentTime - lastStatusTime > 10000) {
    lastStatusTime = currentTime;
    Serial.print(F("[STATUS] Uptime: "));
    Serial.print(currentTime / 1000);
    Serial.print(F("s, RS485 bytes received: "));
    Serial.print(rs485BytesReceived);
    Serial.print(F(", dropped: "));
    Serial.print(rs485BytesDropped);
    Serial.print(F(", RS485 listening: "));
    Serial.println(RS485Serial.isListening() ? "YES" : "NO");
    Serial.print(F("[STATUS] RS485 DE pin: "));
    Serial.print(digitalRead(RS485_DE_PIN) ? "HIGH (TX mode)" : "LOW (RX mode)");
    Serial.print(F(", RE pin: "));
    Serial.println(digitalRead(RS485_RE_PIN) ? "HIGH" : "LOW");
  }
  
  // Check for ANY bytes on RS485 (even if not a complete command)
  // This helps diagnose if the converter is working at all
  if (currentTime - lastRS485CheckTime > 100) {
    lastRS485CheckTime = currentTime;
    int available = RS485Serial.available();
    if (available == 0) {
      // No data available - check if MAX485 might not be powered
      // Only print warning every 10 seconds to avoid spam
      static unsigned long lastPowerWarning = 0;
      if (currentTime - lastPowerWarning > 10000) {
        lastPowerWarning = currentTime;
        Serial.println(F("[RS485] No data received - check MAX485 power (VCC→5V, GND→GND)"));
      }
    }
  }
  
  // PRIORITY 1: Check if command received from RS485 (HIGHEST PRIORITY)
  // CRITICAL: Always process RS485 commands, even if waiting for DWM response
  // If a new command arrives while waiting, cancel the current wait and process new command
  if (RS485Serial.available()) {
    // If we're waiting for DWM response and a new command arrives, cancel the wait
    if (waitingForDWMResponse) {
      Serial.println(F("[RS485] WARNING: New command received while waiting for DWM response - canceling wait"));
      waitingForDWMResponse = false;
      // Switch listener back to RS485 to receive the new command
      if (!RS485Serial.isListening()) {
        RS485Serial.listen();
        delay(10);
      }
      // Ensure RS485 is in RX mode
      set_rs485_rx_mode();
    }
    // Show activity detected and the first byte in hex to help diagnose baud/wiring issues
    int peekByte = RS485Serial.peek();
    unsigned char peekChar = (unsigned char)peekByte;
    Serial.print(F("[RS485] Activity detected (0x"));
    if (peekChar < 0x10) Serial.print('0');
    Serial.print(peekChar, HEX);
    Serial.print(F("="));
    Serial.print((int)peekChar);
    Serial.print(F("='"));
    if (peekChar >= 32 && peekChar < 127) {
      Serial.print((char)peekChar);
    } else if (peekChar == '\r') {
      Serial.print(F("\\r"));
    } else if (peekChar == '\n') {
      Serial.print(F("\\n"));
    } else {
      Serial.print('.');
    }
    Serial.println(F("'), receiving command..."));
    
    if (rs485_receive_command(commandBuffer, COMMAND_BUFFER_SIZE)) {
      // Update last command time for state recovery
      lastCommandTime = millis();
      
      // Valid command received - blink RX LED
      Serial.print(F("[RS485->] Received command: \""));
      Serial.print(commandBuffer);
      Serial.println(F("\""));
      
      blink_rx_led();
      
      // Forward command to DWM3001CDK - TRANSPARENT BRIDGE
      // Send EXACTLY what orchestrator sent - no modification
      Serial.print(F("[->DWM] Forwarding command: \""));
      Serial.print(commandBuffer);
      Serial.println(F("\""));
      
      // CRITICAL: Switch listener to DWM BEFORE sending
      // RS485 is now in RX mode (DE=LOW, RE=LOW), so we can switch to DWM
      if (!DWMSerial.isListening()) {
        DWMSerial.listen();
        delay(10);  // Small delay for listener switch
      }
      
      // Clear any pending data from DWM before sending
      int cleared = 0;
      while (DWMSerial.available()) {
        DWMSerial.read();
        cleared++;
      }
      if (cleared > 0) {
        Serial.print(F("[->DWM] Cleared "));
        Serial.print(cleared);
        Serial.println(F(" bytes from DWM buffer"));
      }
      
      // Send command EXACTLY as orchestrator sent it (commandBuffer already has the command)
      // DWM3001CDK expects: "PNG\r\n", "NODE_TYPE\r\n", etc.
      // We forward the exact command string, then add \r\n terminator
      
      // CRITICAL FIX: Mark that we're waiting for DWM response to prevent listener switching
      waitingForDWMResponse = true;
      
      dwm_send_command(commandBuffer);
      
      // Wait for response from DWM3001CDK
      bool responseReceived = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
      
      // Clear the flag after response (or timeout)
      // CRITICAL: Always clear this flag, even on timeout, to prevent stuck state
      waitingForDWMResponse = false;
      
      // CRITICAL: Ensure RS485 is back in RX mode after sending response
      // Sometimes the mode can get stuck if there was an error
      set_rs485_rx_mode();
      
      // CRITICAL: Switch listener back to RS485 to receive next command
      if (!RS485Serial.isListening()) {
        RS485Serial.listen();
        delay(10);
      }
      
      if (responseReceived) {
        // Valid response received - send to orchestrator
        Serial.print(F("[DWM->] Response: \""));
        Serial.print(responseBuffer);
        Serial.println(F("\""));
        blink_tx_led();
        
        Serial.print(F("[->RS485] Forwarding to orchestrator: \""));
        Serial.print(responseBuffer);
        Serial.println(F("\""));
        rs485_send_response(responseBuffer);
        
        // Clear error LED if it was on
        if (!handshakeComplete) {
          handshakeComplete = true;
          set_error_led(false);
          Serial.println(F("[STATUS] Handshake recovered - Error LED cleared"));
        }
      } else {
        // No response or timeout - send error
        Serial.print(F("[ERROR] DWM3001CDK timeout - no response to \""));
        Serial.print(commandBuffer);
        Serial.println(F("\""));
        send_error("DWM_NO_RESPONSE");
      }
    } else {
      Serial.println(F("[WARN] Failed to receive valid command from RS485"));
    }
  }
  
  // PRIORITY 2: Check for commands from USB Serial Monitor (for debugging)
  // Only process if no RS485 activity to avoid conflicts
  // Works like Command_Test.ino - simple command forwarding
  // Note: RS485 commands always take priority over USB serial commands
  if (!RS485Serial.available() && !waitingForDWMResponse && Serial.available()) {
    static char serialCommandBuffer[COMMAND_BUFFER_SIZE];
    if (serial_receive_command(serialCommandBuffer, COMMAND_BUFFER_SIZE)) {
      // Check for special commands
      if (strcmp(serialCommandBuffer, "T") == 0 || strcmp(serialCommandBuffer, "t") == 0) {
        // Toggle test mode
        testModeEnabled = !testModeEnabled;
        Serial.print(F("[TEST] Test mode "));
        Serial.println(testModeEnabled ? "ENABLED" : "DISABLED");
        if (testModeEnabled) {
          testCommandIndex = 0;
          lastTestTime = millis();
        }
      } else {
        // Forward command to DWM3001CDK for debugging (like Command_Test.ino)
        handle_serial_command(serialCommandBuffer);
      }
    }
  }
  
  // Run test mode if enabled (only when not handling RS485)
  if (testModeEnabled && !waitingForDWMResponse && !RS485Serial.available()) {
    test_dwm_communication();
  }
  
  // Small delay to prevent CPU spinning
  delay(10);
}

// ============================================================================
// RS485 CONTROL FUNCTIONS
// ============================================================================

/**
 * Set RS485 transceiver to transmit mode
 * DE=HIGH, RE=HIGH (enable driver, disable receiver)
 * Used when Arduino needs to send data to orchestrator
 */
void set_rs485_tx_mode() {
  digitalWrite(RS485_DE_PIN, HIGH);   // Enable driver (transmit)
  digitalWrite(RS485_RE_PIN, HIGH);   // Disable receiver
  delayMicroseconds(RS485_TX_DELAY_US);  // Wait for transceiver to switch
}

/**
 * Set RS485 transceiver to receive mode
 * DE=LOW, RE=LOW (disable driver, enable receiver)
 * Used when Arduino is waiting to receive commands from orchestrator
 */
void set_rs485_rx_mode() {
  delayMicroseconds(RS485_TX_DELAY_US);  // Wait for last bit to transmit (if switching from TX)
  digitalWrite(RS485_DE_PIN, LOW);    // Disable driver (receive)
  digitalWrite(RS485_RE_PIN, LOW);    // Enable receiver
}

// ============================================================================
// RS485 COMMUNICATION FUNCTIONS
// ============================================================================

/**
 * Receive command from RS485 (orchestrator)
 * Reads until \r or \n is received
 * Returns true if valid command received, false otherwise
 */
bool rs485_receive_command(char* buffer, int maxLen) {
  int index = 0;
  unsigned long startTime = millis();
  bool firstChar = true;
  
  while (index < maxLen - 1) {
    if (RS485Serial.available()) {
      int byteRead = RS485Serial.read();
      if (byteRead == -1) {
        // No data available (shouldn't happen after available() check, but handle it)
        continue;
      }
      unsigned char c = (unsigned char)byteRead;  // Cast to unsigned to avoid sign extension issues
      rs485BytesReceived++;
      
      // Detect baud rate mismatch - corrupted bytes usually have high values or are 0xFF
      if (firstChar && (c == 0xFF || c > 0x7F)) {
        Serial.println(F("[ERROR] BAUD RATE MISMATCH - Received corrupted byte"));
        Serial.print(F("[ERROR] Arduino receiving at: "));
        Serial.print(RS485_BAUD_RATE);
        Serial.println(F(" baud - Check orchestrator baud rate!"));
      }
      firstChar = false;
      
      // Check for command terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {  // Only accept if we have data
          buffer[index] = '\0';  // Null terminate
          return true;
        }
        // Skip leading/empty terminators
      } else {
        buffer[index++] = c;
        
        // Detect baud rate mismatch - corrupted bytes usually have high values or are 0xFF
        if (c == 0xFF || (c >= 0xE0 && c <= 0xFF)) {
          Serial.println(F("[ERROR] BAUD RATE MISMATCH - Received corrupted byte"));
          Serial.print(F("[ERROR] Arduino receiving at: "));
          Serial.print(RS485_BAUD_RATE);
          Serial.println(F(" baud - Check orchestrator baud rate!"));
        }
      }
      
      startTime = millis();  // Reset timeout on each character
    }
    
    // Timeout check (1 second for command reception)
    if (millis() - startTime > 1000) {
      if (index > 0) {
        buffer[index] = '\0';
        Serial.print(F("[WARN] RS485 command timeout, partial received: \""));
        Serial.print(buffer);
        Serial.println(F("\""));
        return true;  // Return partial command
      }
      Serial.println(F("[WARN] RS485 command receive timeout - no data received"));
      return false;
    }
  }
  
  // Buffer full (should not reach here due to while condition, but handle it)
  buffer[maxLen - 1] = '\0';
  rs485BytesDropped++;
  Serial.print(F("[ERROR] RS485 command buffer full! Dropped bytes. Partial: \""));
  Serial.print(buffer);
  Serial.println(F("\""));
  return false;
}

/**
 * Send response to RS485 (orchestrator)
 * Automatically handles DE/RE control
 */
void rs485_send_response(const char* response) {
  set_rs485_tx_mode();
  
  // Send response string
  RS485Serial.print(response);
  
  // Add line terminators if not already present
  int len = strlen(response);
  if (len == 0 || (response[len-1] != '\n' && response[len-1] != '\r')) {
    RS485Serial.print("\r\n");
  }
  
  // Wait for transmission to complete (blocking in SoftwareSerial)
  // RS485Serial.flush();
  
  // CRITICAL: Always return to RX mode immediately after sending
  // This ensures RS485 is ready to receive the next command at any time
  set_rs485_rx_mode();
  
  // Update last transmit time for mode checking
  lastTransmitTime = millis();
}

// ============================================================================
// DWM3001CDK COMMUNICATION FUNCTIONS
// ============================================================================

/**
 * Send command to DWM3001CDK
 */
/**
 * Send command to DWM3001CDK
 * TRANSPARENT BRIDGE: Sends exactly what orchestrator sent
 * Format: command + \r\n (same as orchestrator sends)
 */
void dwm_send_command(const char* command) {
  // CRITICAL FIX: Ensure DWM serial is listening and verify it stays listening
  if (!DWMSerial.isListening()) {
    DWMSerial.listen();
    delay(20);  // Increased delay to ensure listener switch completes
    
    // Verify the switch was successful
    if (!DWMSerial.isListening()) {
      Serial.println(F("[ERROR] Failed to switch to DWM serial listener!"));
      // Try one more time
      DWMSerial.listen();
      delay(20);
    }
  }
  
  // Send command exactly as orchestrator sent it
  // Orchestrator sends: command + \r\n
  // We forward: command + \r\n (identical format)
  
  DWMSerial.print(command);
  DWMSerial.print("\r\n");  // Add terminator (same as orchestrator)
  
  // CRITICAL FIX: Increased delay to ensure transmission completes
  // SoftwareSerial at 115200 baud needs more time:
  // - Each byte takes ~87us at 115200 baud
  // - For "PNG\r\n" (5 bytes) = ~435us minimum
  // - Add margin for SoftwareSerial overhead and DWM processing time
  // - Also need time for DWM to process command and start responding
  // - SoftwareSerial needs additional time to complete bit transmission
  delay(50);  // Increased to 50ms to ensure reliable transmission and DWM processing
}

/**
 * Receive response from DWM3001CDK
 * Reads until \r or \n is received
 * Returns true if valid response received, false on timeout
 */
bool dwm_receive_response(char* buffer, int maxLen) {
  int index = 0;
  unsigned long startTime = millis();
  bool firstChar = true;
  int bytesReceived = 0;
  
  Serial.print(F("[DWM RX] Waiting for response (timeout="));
  Serial.print(DWM_RESPONSE_TIMEOUT_MS);
  Serial.println(F("ms)..."));
  
  // CRITICAL FIX: Ensure DWM serial is listening and STAYS listening
  // Don't allow listener to switch away during response wait
  if (!DWMSerial.isListening()) {
    Serial.println(F("[DWM RX] WARNING: DWM serial not listening! Switching..."));
    DWMSerial.listen();
    delay(20);  // Increased delay to ensure listener switch completes
  }
  
  // CRITICAL FIX: Small delay after ensuring listener is set
  // This allows SoftwareSerial to stabilize before we start reading
  delay(10);
  
  // Check initial availability
  int initialAvailable = DWMSerial.available();
  if (initialAvailable > 0) {
    Serial.print(F("[DWM RX] Already have "));
    Serial.print(initialAvailable);
    Serial.println(F(" bytes available"));
  }
  
  unsigned long lastByteTime = startTime;  // Track when we last received a byte
  
  while (index < maxLen - 1) {
    // CRITICAL: Check if new RS485 command arrived - if so, abort waiting for DWM response
    // This allows commands to be sent at any time
    if (RS485Serial.available() && waitingForDWMResponse) {
      Serial.println(F("[DWM RX] New RS485 command detected - aborting DWM response wait"));
      return false;  // Abort waiting, new command takes priority
    }
    
    // CRITICAL FIX: Periodically verify listener hasn't switched away
    // This prevents missing data if listener gets switched by main loop
    if (!DWMSerial.isListening() && waitingForDWMResponse) {
      Serial.println(F("[WARN] Listener was switched away - restoring DWM listener"));
      DWMSerial.listen();
      delay(20);  // Increased delay for listener restoration
    }
    
      // #region agent log
      // Log periodic status every 500ms to track progress
      unsigned long currentCheckTime = millis();
      static unsigned long lastStatusLog = 0;
      if (currentCheckTime - lastStatusLog > 500) {
        lastStatusLog = currentCheckTime;
        int currentAvailable = DWMSerial.available();
        Serial.print(F("[DEBUG] Still waiting: elapsed="));
        Serial.print(currentCheckTime - startTime);
        Serial.print(F("ms, available="));
        Serial.print(currentAvailable);
        Serial.print(F(", index="));
        Serial.print(index);
        Serial.print(F(", listening="));
        Serial.print(DWMSerial.isListening() ? "YES" : "NO");
        Serial.print(F(", bytesReceived="));
        Serial.print(bytesReceived);
        Serial.print(F(", lastByteTime="));
        Serial.print(currentCheckTime - lastByteTime);
        Serial.println(F("ms ago"));
        Serial.flush();  // CRITICAL: Force output
      
      // If we have bytes available but haven't read them, that's suspicious
      if (currentAvailable > 0 && bytesReceived == 0) {
        Serial.println(F("[DEBUG] WARNING: Bytes available but not being read! Possible blocking issue."));
        // Try to peek at what's there
        char peekChar = DWMSerial.peek();
        Serial.print(F("[DEBUG] Peek at first byte: 0x"));
        if ((unsigned char)peekChar < 0x10) Serial.print('0');
        Serial.print((unsigned char)peekChar, HEX);
        Serial.print(F(" ('"));
        if (peekChar >= 32 && peekChar < 127) Serial.print(peekChar);
        Serial.println(F("')"));
      }
    }
    // #endregion
    
  // CRITICAL: Check for new RS485 command before reading DWM data
  // New commands always take priority
  if (RS485Serial.available() && waitingForDWMResponse) {
    Serial.println(F("[DWM RX] New RS485 command detected during read - aborting"));
    return false;  // Abort, new command takes priority
  }
  
  // CRITICAL FIX: More aggressive reading - check available() more frequently
  // SoftwareSerial can miss bytes if we don't read fast enough at 115200 baud
  int availableNow = DWMSerial.available();
  if (availableNow > 0) {
      // #region agent log
      // Log when bytes become available (first time only)
      if (bytesReceived == 0 && availableNow > 0) {
        Serial.print(F("[DEBUG] BYTES DETECTED! available()="));
        Serial.print(availableNow);
        Serial.print(F(" at elapsed="));
        Serial.print(millis() - startTime);
        Serial.println(F("ms"));
        Serial.flush();
      }
      // #endregion
      
      // CRITICAL FIX: Read immediately when available
      // SoftwareSerial can lose bytes if we don't read fast enough at 115200 baud
      char c = DWMSerial.read();
      bytesReceived++;
      
      if (firstChar) {
        firstChar = false;  // Mark that we've received the first byte
      }
      
      // CRITICAL FIX: Less aggressive filtering - accept more characters
      // Some DWM responses might have control characters we need to handle
      // Only filter truly invalid characters (null bytes)
      if (c == 0x00) {
        continue;  // Skip null bytes only
      }
      
      // Check for response terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {  // Only accept if we have data
          buffer[index] = '\0';  // Null terminate
          Serial.print(F("[DWM RX] Response complete: \""));
          Serial.print(buffer);
          Serial.print(F("\" ("));
          Serial.print(index);
          Serial.println(F(" chars)"));
          return true;
        }
        // Empty line, continue reading
      } else {
        buffer[index++] = c;
      }
      
      lastByteTime = millis();  // Track when we last received a byte
      startTime = millis();  // Reset timeout on each character
    } else {
      // CRITICAL: Check for new RS485 command even when no DWM data available
      // This ensures commands can interrupt waiting for DWM response
      if (RS485Serial.available() && waitingForDWMResponse) {
        Serial.println(F("[DWM RX] New RS485 command detected - aborting wait"));
        return false;  // Abort waiting, process new command
      }
      
      // CRITICAL FIX: Small delay when no data available
      // This prevents tight looping and allows SoftwareSerial interrupt handlers to run
      // SoftwareSerial uses interrupts, so we need to yield CPU time
      delay(1);  // 1ms delay allows interrupt handlers to process incoming bytes
    }
    
    // Timeout check
    unsigned long elapsed = millis() - startTime;
    if (elapsed > DWM_RESPONSE_TIMEOUT_MS) {
      if (index > 0) {
        buffer[index] = '\0';
        // Only return partial response if it looks valid (starts with "OK" or "ERR")
        if (strncmp(buffer, "OK", 2) == 0 || strncmp(buffer, "ERR", 3) == 0) {
          Serial.print(F("[WARN] DWM response timeout, partial: \""));
          Serial.print(buffer);
          Serial.print(F("\" ("));
          Serial.print(index);
          Serial.println(F(" chars)"));
          return true;  // Return partial response if it looks valid
        } else {
          Serial.print(F("[WARN] DWM response timeout, invalid partial: \""));
          Serial.print(buffer);
          Serial.print(F("\" ("));
          Serial.print(index);
          Serial.println(F(" chars) - ignoring"));
          // Don't return invalid partial responses
        }
      }
      Serial.print(F("[WARN] DWM response timeout - no data received (checked for "));
      Serial.print(elapsed);
      Serial.print(F("ms, bytes seen: "));
      Serial.print(bytesReceived);
      Serial.println(F(")"));
      
      return false;  // No response
    }
  }
  
  // Buffer full
  buffer[maxLen - 1] = '\0';
  Serial.print(F("[ERROR] DWM response buffer full ("));
  Serial.print(index);
  Serial.println(F(" chars)"));
  return false;
}

// ============================================================================
// HANDSHAKE FUNCTION
// ============================================================================

/**
 * Perform startup handshake with DWM3001CDK
 * Sends NT (Node Type) command and waits for response
 * Returns true if successful, false otherwise
 */
bool perform_handshake() {
  // Wait for DWM3001CDK to stabilize
  delay(500);
  
  // Clear any pending data
  int cleared = 0;
  while (DWMSerial.available()) {
    DWMSerial.read();
    cleared++;
  }
  if (cleared > 0) {
    Serial.print(F("[HANDSHAKE] Cleared "));
    Serial.print(cleared);
    Serial.println(F(" bytes from DWM serial buffer"));
  }
  
  // Try handshake multiple times
  for (int attempt = 0; attempt < HANDSHAKE_RETRY_COUNT; attempt++) {
    Serial.print(F("[HANDSHAKE] Attempt "));
    Serial.print(attempt + 1);
    Serial.print(F("/"));
    Serial.print(HANDSHAKE_RETRY_COUNT);
    Serial.println(F(": Sending NODE_TYPE command..."));
    
    // CRITICAL: Ensure DWM serial is listening
    if (!DWMSerial.isListening()) {
      Serial.println(F("[HANDSHAKE] Switching listener to DWM serial..."));
      DWMSerial.listen();
      delay(10);
    }
    
    // Clear any pending data (including null bytes and garbage)
    int cleared = 0;
    unsigned long clearStart = millis();
    while (DWMSerial.available() || (millis() - clearStart < 100)) {
      if (DWMSerial.available()) {
        DWMSerial.read();  // Discard any garbage data
        cleared++;
        clearStart = millis();  // Reset timer on each byte
      }
    }
    if (cleared > 0) {
      Serial.print(F("[HANDSHAKE] Cleared "));
      Serial.print(cleared);
      Serial.println(F(" bytes before handshake"));
    }
    
    // Wait a bit after clearing to let any initialization noise settle
    delay(100);
    
    // Send Node Type query (use NODE_TYPE to match DWM firmware command parser)
    Serial.println(F("[HANDSHAKE] Sending NODE_TYPE command..."));
    
    // CRITICAL FIX: Mark that we're waiting for response to prevent listener switching
    waitingForDWMResponse = true;
    
    dwm_send_command("NODE_TYPE");
    delay(150);  // Increased delay after sending to allow DWM to process
    
    // Wait for response
    Serial.println(F("[HANDSHAKE] Waiting for response..."));
    bool handshakeResponse = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
    
      // Clear the flag after response
      waitingForDWMResponse = false;
      
      // CRITICAL: Ensure RS485 is back in RX mode
      set_rs485_rx_mode();
      
      // CRITICAL: Switch listener back to RS485
      if (!RS485Serial.isListening()) {
        RS485Serial.listen();
        delay(10);
      }
    
    if (handshakeResponse) {
      Serial.print(F("[HANDSHAKE] Received: "));
      Serial.println(responseBuffer);
      
      // Check if response is valid (should be "OK NODE_TYPE=TX_V2" or "OK NODE_TYPE=RX_V2")
      if (strncmp(responseBuffer, "OK ", 3) == 0) {
        // Extract node type from "OK NODE_TYPE=TX_V2" or "OK NODE_TYPE=RX_V2"
        const char* nodeTypeStart = strstr(responseBuffer, "=");
        if (nodeTypeStart != NULL) {
          // Extract everything after "="
          strncpy(nodeType, nodeTypeStart + 1, sizeof(nodeType) - 1);
          nodeType[sizeof(nodeType) - 1] = '\0';
        } else {
          // Fallback: try to extract from "OK TX_V2" format (legacy)
          strncpy(nodeType, responseBuffer + 3, sizeof(nodeType) - 1);
          nodeType[sizeof(nodeType) - 1] = '\0';
        }
        Serial.print(F("[HANDSHAKE] Success! Node type: "));
        Serial.println(nodeType);
        return true;
      } else {
        Serial.print(F("[HANDSHAKE] Invalid response format: "));
        Serial.println(responseBuffer);
      }
    } else {
      Serial.println(F("[HANDSHAKE] No response received"));
    }
    
    // Wait before retry
    if (attempt < HANDSHAKE_RETRY_COUNT - 1) {
      Serial.println(F("[HANDSHAKE] Retrying in 500ms..."));
      delay(500);
    }
  }
  
  // Handshake failed
  Serial.println(F("[HANDSHAKE] All attempts failed"));
  strcpy(nodeType, "UNKNOWN");
  return false;
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * Send error response to orchestrator
 */
void send_error(const char* errorType) {
  char errorMsg[32];  // Reduced from 64 to save memory
  snprintf(errorMsg, sizeof(errorMsg), "ERR_%s", errorType);
  Serial.print(F("[ERROR] Sending error: "));
  Serial.println(errorMsg);
  rs485_send_response(errorMsg);
  
  // Blink error LED
  digitalWrite(LED_ERROR_PIN, HIGH);
  delay(100);
  digitalWrite(LED_ERROR_PIN, LOW);
}

// ============================================================================
// LED CONTROL FUNCTIONS
// ============================================================================

/**
 * Blink RX activity LED
 */
void blink_rx_led() {
  digitalWrite(LED_RX_PIN, HIGH);
  ledRxActive = true;
  ledRxOffTime = millis() + LED_BLINK_DURATION_MS;
}

/**
 * Blink TX activity LED
 */
void blink_tx_led() {
  digitalWrite(LED_TX_PIN, HIGH);
  ledTxActive = true;
  ledTxOffTime = millis() + LED_BLINK_DURATION_MS;
}

/**
 * Set error LED state
 */
void set_error_led(bool state) {
  digitalWrite(LED_ERROR_PIN, state ? HIGH : LOW);
}

/**
 * Update heartbeat LED (1Hz blink)
 */
void update_heartbeat() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
    heartbeatState = !heartbeatState;
    digitalWrite(LED_HEARTBEAT_PIN, heartbeatState ? HIGH : LOW);
    lastHeartbeatTime = currentTime;
  }
}

/**
 * Update activity LEDs (turn off after blink duration)
 */
void update_activity_leds() {
  unsigned long currentTime = millis();
  
  // Turn off RX LED after blink duration
  if (ledRxActive && currentTime >= ledRxOffTime) {
    digitalWrite(LED_RX_PIN, LOW);
    ledRxActive = false;
  }
  
  // Turn off TX LED after blink duration
  if (ledTxActive && currentTime >= ledTxOffTime) {
    digitalWrite(LED_TX_PIN, LOW);
    ledTxActive = false;
  }
}

// ============================================================================
// TEST MODE FUNCTION
// ============================================================================

/**
 * Test DWM communication by sending test commands
 * Called every 5 seconds when test mode is enabled
 */
void test_dwm_communication() {
  unsigned long currentTime = millis();
  
  // Send test command every 5 seconds
  if (currentTime - lastTestTime >= 5000) {
    lastTestTime = currentTime;
    
    if (testCommandIndex >= testCommandCount) {
      testCommandIndex = 0;
    }
    
    const char* testCmd = testCommands[testCommandIndex];
    Serial.println(F("=========================================="));
    Serial.print(F("[TEST] Sending test command #"));
    Serial.print(testCommandIndex + 1);
    Serial.print(F(": \""));
    Serial.print(testCmd);
    Serial.println(F("\""));
    Serial.println(F("=========================================="));
    
    // CRITICAL FIX: Ensure DWM listener is active and prevent switching during test
    waitingForDWMResponse = true;
    
    // Clear DWM buffer before sending
    DWMSerial.listen();
    delay(20);  // Increased delay for listener switch
    int cleared = 0;
    while (DWMSerial.available()) {
      DWMSerial.read();
      cleared++;
    }
    if (cleared > 0) {
      Serial.print(F("[TEST] Cleared "));
      Serial.print(cleared);
      Serial.println(F(" bytes from DWM buffer"));
    }
    
    // Send command
    dwm_send_command(testCmd);
    
    // Wait for response
    bool responseReceived = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
    
    // #region agent log
    Serial.print(F("[DEBUG] dwm_receive_response returned: "));
    Serial.println(responseReceived ? "true" : "false");
    Serial.flush();
    // #endregion
    
      // Clear the flag after response
      waitingForDWMResponse = false;
      
      // CRITICAL: Ensure RS485 is back in RX mode
      set_rs485_rx_mode();
      
      // CRITICAL: Switch listener back to RS485
      if (!RS485Serial.isListening()) {
        RS485Serial.listen();
        delay(10);
      }
    
    if (responseReceived) {
      Serial.print(F("[TEST] ✅ SUCCESS - Response: \""));
      Serial.print(responseBuffer);
      Serial.println(F("\""));
      blink_tx_led();
    } else {
      Serial.println(F("[TEST] ❌ FAILED - No response received"));
      blink_rx_led();  // Use RX LED to indicate failure
    }
    
    testCommandIndex++;
    Serial.println(F("==========================================\n"));
  }
}

// ============================================================================
// SERIAL MONITOR COMMAND HANDLING (FOR DEBUGGING)
// ============================================================================

/**
 * Receive command from USB Serial Monitor
 * Reads until \r or \n is received
 * Returns true if valid command received, false otherwise
 */
bool serial_receive_command(char* buffer, int maxLen) {
  int index = 0;
  unsigned long startTime = millis();
  
  while (index < maxLen - 1) {
    if (Serial.available()) {
      char c = Serial.read();
      
      // Check for command terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {  // Only accept if we have data
          buffer[index] = '\0';  // Null terminate
          return true;
        }
        // Skip leading/empty terminators
        continue;
      } else {
        buffer[index++] = c;
      }
      
      startTime = millis();  // Reset timeout on each character
    }
    
    // Timeout check (1 second for command reception)
    if (millis() - startTime > 1000) {
      if (index > 0) {
        buffer[index] = '\0';
        return true;  // Return partial command
      }
      return false;
    }
  }
  
  // Buffer full
  buffer[maxLen - 1] = '\0';
  return false;
}

/**
 * Handle command received from Serial Monitor
 * Forwards command to DWM3001CDK and displays response
 */
void handle_serial_command(const char* command) {
  Serial.println(F("=========================================="));
  Serial.print(F("[SERIAL] Command received: \""));
  Serial.print(command);
  Serial.println(F("\""));
  Serial.println(F("[SERIAL] Forwarding to DWM3001CDK..."));
  
  // CRITICAL: Switch listener to DWM BEFORE sending
  if (!DWMSerial.isListening()) {
    DWMSerial.listen();
    delay(10);
  }
  
  // Clear any pending data from DWM before sending
  int cleared = 0;
  while (DWMSerial.available()) {
    DWMSerial.read();
    cleared++;
  }
  if (cleared > 0) {
    Serial.print(F("[SERIAL] Cleared "));
    Serial.print(cleared);
    Serial.println(F(" bytes from DWM buffer"));
  }
  
  // Mark that we're waiting for DWM response
  waitingForDWMResponse = true;
  
  // Send command to DWM3001CDK
  dwm_send_command(command);
  
  // Wait for response
  bool responseReceived = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
  
      // Clear the flag after response
      waitingForDWMResponse = false;
      
      // CRITICAL: Ensure RS485 is back in RX mode
      set_rs485_rx_mode();
      
      // CRITICAL: Switch listener back to RS485
      if (!RS485Serial.isListening()) {
        RS485Serial.listen();
        delay(10);
      }
  
  if (responseReceived) {
    Serial.print(F("[SERIAL] ✅ Response: \""));
    Serial.print(responseBuffer);
    Serial.println(F("\""));
  } else {
    Serial.println(F("[SERIAL] ❌ No response received (timeout)"));
  }
  
  Serial.println(F("==========================================\n"));
}

// ============================================================================
// STATE RECOVERY FUNCTION
// ============================================================================

/**
 * Recover from stuck states
 * Called periodically to ensure bridge is in correct state
 */
void recover_state() {
  unsigned long currentTime = millis();
  
  Serial.println(F("[STATE] Performing state recovery check..."));
  
  // 1. Check if waitingForDWMResponse flag is stuck
  // If it's been set for more than DWM_RESPONSE_TIMEOUT_MS + 1 second, it's stuck
  static unsigned long waitingStartTime = 0;
  if (waitingForDWMResponse) {
    if (waitingStartTime == 0) {
      waitingStartTime = currentTime;
    } else if (currentTime - waitingStartTime > (DWM_RESPONSE_TIMEOUT_MS + 1000)) {
      Serial.println(F("[STATE] WARNING: waitingForDWMResponse flag stuck - clearing"));
      waitingForDWMResponse = false;
      waitingStartTime = 0;
    }
  } else {
    waitingStartTime = 0;
  }
  
  // 2. Ensure RS485 is in RX mode (should ALWAYS be in RX mode when not actively transmitting)
  // RS485 must be ready to receive commands at any time
  bool deState = digitalRead(RS485_DE_PIN);
  bool reState = digitalRead(RS485_RE_PIN);
  if (deState == HIGH || reState == HIGH) {
    Serial.println(F("[STATE] RS485 not in RX mode - forcing RX mode (must be ready for commands)"));
    set_rs485_rx_mode();
    // If RS485 was stuck in TX mode, we might have missed commands
    // Check if there's data waiting now that we're in RX mode
    delay(10);  // Small delay for mode switch to complete
    if (RS485Serial.available()) {
      Serial.print(F("[STATE] Found "));
      Serial.print(RS485Serial.available());
      Serial.println(F(" bytes waiting after switching to RX mode"));
    }
  }
  
  // 3. Ensure RS485 listener is active (always ready to receive commands)
  // Commands can arrive at any time, so RS485 should always be listening
  if (!RS485Serial.isListening()) {
    Serial.println(F("[STATE] RS485 listener not active - switching (commands must always be receivable)"));
    RS485Serial.listen();
    delay(10);
    // If we were waiting for DWM, cancel it - new command takes priority
    if (waitingForDWMResponse) {
      Serial.println(F("[STATE] Canceling DWM wait - RS485 must be ready for commands"));
      waitingForDWMResponse = false;
    }
  }
  
  // 4. Clear any stuck buffers
  // If there's data in RS485 buffer but no command is being processed, clear it
  if (!waitingForDWMResponse && RS485Serial.available() > 100) {
    Serial.print(F("[STATE] Clearing stuck RS485 buffer ("));
    Serial.print(RS485Serial.available());
    Serial.println(F(" bytes)"));
    int cleared = 0;
    while (RS485Serial.available() && cleared < 200) {
      RS485Serial.read();
      cleared++;
    }
    Serial.print(F("[STATE] Cleared "));
    Serial.print(cleared);
    Serial.println(F(" bytes"));
  }
  
  // 5. Check if DWM listener is stuck
  // RS485 should always be listening unless we're actively waiting for DWM response
  // If DWM listener is active but no response is expected, switch back to RS485
  if (DWMSerial.isListening() && !RS485Serial.isListening()) {
    if (!waitingForDWMResponse) {
      Serial.println(F("[STATE] DWM listener active but not needed - switching to RS485"));
      RS485Serial.listen();
      delay(10);
    } else {
      // We're waiting for DWM, but check if RS485 has a new command (takes priority)
      if (RS485Serial.available()) {
        Serial.println(F("[STATE] New RS485 command while waiting for DWM - canceling wait"));
        waitingForDWMResponse = false;
        RS485Serial.listen();
        delay(10);
      }
    }
  }
  
  // 6. Reset lastCommandTime if no command received for too long
  if (currentTime - lastCommandTime > COMMAND_TIMEOUT_MS && lastCommandTime > 0) {
    Serial.println(F("[STATE] No commands received for extended period - resetting state"));
    // Force state reset
    waitingForDWMResponse = false;
    set_rs485_rx_mode();
    RS485Serial.listen();
    delay(10);
    lastCommandTime = 0;  // Reset to prevent repeated messages
  }
  
  Serial.println(F("[STATE] State recovery check complete"));
}
