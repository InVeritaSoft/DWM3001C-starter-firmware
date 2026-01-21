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

// Auto-ping mode: Send PNG to DWM every 5 seconds (always active, not just in test mode)
unsigned long lastAutoPingTime = 0;
#define AUTO_PING_INTERVAL_MS 5000  // 5 seconds

// CRITICAL: Flag to prevent listener switching during DWM communication
bool waitingForDWMResponse = false;

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
  Serial.println(F("[TEST] Send 'T' via USB Serial to enable test mode"));
  Serial.println(F("[TEST] Test mode sends commands to DWM every 5 seconds"));
  Serial.println(F("==========================================\n"));
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Check for test mode command via USB Serial
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'T' || cmd == 't') {
      testModeEnabled = !testModeEnabled;
      Serial.print(F("[TEST] Test mode "));
      Serial.println(testModeEnabled ? "ENABLED" : "DISABLED");
      if (testModeEnabled) {
        testCommandIndex = 0;
        lastTestTime = millis();
      }
    }
  }
  
  // Run test mode if enabled
  if (testModeEnabled) {
    test_dwm_communication();
  }
  
  // AUTO-PING: Send PNG command to DWM every 5 seconds (always active)
  if (millis() - lastAutoPingTime >= AUTO_PING_INTERVAL_MS) {
    lastAutoPingTime = millis();
    
    Serial.println(F("=========================================="));
    Serial.println(F("[AUTO-PING] Sending PNG to DWM..."));
    Serial.println(F("=========================================="));
    
    // Set flag to prevent listener switching during communication
    waitingForDWMResponse = true;
    
    // Clear DWM buffer before sending
    DWMSerial.listen();
    delay(20);
    int cleared = 0;
    while (DWMSerial.available()) {
      DWMSerial.read();
      cleared++;
    }
    if (cleared > 0) {
      Serial.print(F("[AUTO-PING] Cleared "));
      Serial.print(cleared);
      Serial.println(F(" bytes from DWM buffer"));
    }
    
    // Send PNG command
    dwm_send_command("PNG");
    
    // Wait for response
    bool responseReceived = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
    
    // Clear the flag
    waitingForDWMResponse = false;
    
    if (responseReceived) {
      Serial.print(F("[AUTO-PING] ✅ Response: \""));
      Serial.print(responseBuffer);
      Serial.println(F("\""));
      
      // Forward successful ping result to orchestrator via RS485
      // Format: OK PING <response_from_dwm>
      char pingResponse[128];
      snprintf(pingResponse, sizeof(pingResponse), "OK PING %s", responseBuffer);
      rs485_send_response(pingResponse);
      Serial.print(F("[AUTO-PING] → Sent to orchestrator: \""));
      Serial.print(pingResponse);
      Serial.println(F("\""));
    } else {
      Serial.println(F("[AUTO-PING] ❌ No response received"));
      
      // Forward timeout error to orchestrator via RS485
      rs485_send_response("ERR PING_TIMEOUT");
      Serial.println(F("[AUTO-PING] → Sent to orchestrator: ERR PING_TIMEOUT"));
    }
    Serial.println();
  }
  
  // BACKGROUND LISTENER: Catch autonomous messages from DWM (like "OK PING" every 5s)
  // Only check when not actively waiting for a response to avoid conflicts
  if (!waitingForDWMResponse) {
    static char dwmBackgroundBuffer[256] = {0};
    static int dwmBackgroundIndex = 0;
    static unsigned long lastDWMBackgroundCheck = 0;
    
    // Check for incoming data from DWM every 100ms (faster than diagnostic check)
    if (millis() - lastDWMBackgroundCheck > 100) {
      lastDWMBackgroundCheck = millis();
      
      bool wasListening = RS485Serial.isListening();
      // #region agent log - listener check
      static unsigned long lastListenerLog = 0;
      if (millis() - lastListenerLog > 1000) {  // Log every 1 second
        lastListenerLog = millis();
        Serial.print(F("[DWM-AUTO-DBG] Listener check: RS485="));
        Serial.print(wasListening ? "YES" : "NO");
        Serial.print(F(", DWM="));
        Serial.println(DWMSerial.isListening() ? "YES" : "NO");
      }
      // #endregion
      if (wasListening) {
        // Temporarily switch to DWM to check for data
        DWMSerial.listen();
        delay(5);  // Small delay for listener switch
      }
      
      int availableBefore = DWMSerial.available();
      // #region agent log - bytes available
      if (availableBefore > 0) {
        Serial.print(F("[DWM-AUTO-DBG] Found "));
        Serial.print(availableBefore);
        Serial.println(F(" bytes available from DWM"));
      }
      // #endregion
      
      // Read available bytes from DWM
      int bytesRead = 0;
      while (DWMSerial.available() && dwmBackgroundIndex < sizeof(dwmBackgroundBuffer) - 1) {
        char c = DWMSerial.read();
        bytesRead++;
        
        // #region agent log - byte received
        Serial.print(F("[DWM-AUTO-DBG] Byte #"));
        Serial.print(bytesRead);
        Serial.print(F(": 0x"));
        if ((unsigned char)c < 0x10) Serial.print('0');
        Serial.print((unsigned char)c, HEX);
        Serial.print(F(" '"));
        if (c >= 32 && c < 127) Serial.print(c);
        Serial.println(F("'"));
        // #endregion
        
        // Check for message terminator (\r or \n)
        if (c == '\r' || c == '\n') {
          if (dwmBackgroundIndex > 0) {
            // Complete message received - null terminate and process
            dwmBackgroundBuffer[dwmBackgroundIndex] = '\0';
            
            Serial.print(F("[DWM-AUTO] ✅ Caught autonomous message: \""));
            Serial.print(dwmBackgroundBuffer);
            Serial.println(F("\""));
            
            // Forward to orchestrator
            char autoResponse[128];
            snprintf(autoResponse, sizeof(autoResponse), "OK AUTO %s", dwmBackgroundBuffer);
            rs485_send_response(autoResponse);
            Serial.print(F("[DWM-AUTO] → Sent to orchestrator: \""));
            Serial.print(autoResponse);
            Serial.println(F("\""));
            
            // Reset buffer
            dwmBackgroundIndex = 0;
            dwmBackgroundBuffer[0] = '\0';
          }
        } else if (c >= 32 && c < 127) {
          // Valid ASCII character - add to buffer
          dwmBackgroundBuffer[dwmBackgroundIndex++] = c;
        }
        // Ignore other control characters
      }
      
      // Restore listener if we switched it
      if (wasListening) {
        RS485Serial.listen();
      }
    }
  }
  
  // CRITICAL DIAGNOSTIC: Periodically check if ANY bytes are arriving from DWM
  // This helps diagnose if the connection is working at all
  static unsigned long lastDWMCheck = 0;
  if (millis() - lastDWMCheck > 2000) {  // Check every 2 seconds
    lastDWMCheck = millis();
    bool wasListening = DWMSerial.isListening();
    if (!wasListening) {
      DWMSerial.listen();
      delay(10);
    }
    int dwmBytes = DWMSerial.available();
    if (dwmBytes > 0) {
      Serial.print(F("[DIAG] DWM has "));
      Serial.print(dwmBytes);
      Serial.println(F(" bytes waiting (outside command cycle)!"));
      Serial.flush();
      // Read and display them
      for (int i = 0; i < dwmBytes && i < 10; i++) {
        char c = DWMSerial.read();
        Serial.print(F("[DIAG] DWM byte: 0x"));
        if ((unsigned char)c < 0x10) Serial.print('0');
        Serial.print((unsigned char)c, HEX);
        Serial.print(F(" '"));
        if (c >= 32 && c < 127) Serial.print(c);
        Serial.println(F("'"));
        Serial.flush();
      }
    }
    if (!wasListening) {
      RS485Serial.listen();  // Restore RS485 listener
    }
  }
  
  // CRITICAL FIX: Only switch to RS485 if we're not currently waiting for DWM response
  // Don't switch listener while DWM communication is in progress
  // This prevents missing DWM responses due to listener switching
  if (!waitingForDWMResponse && !RS485Serial.isListening()) {
    RS485Serial.listen();
    Serial.println(F("[RS485] Switched listener to RS485 port"));
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
    if (available > 0) {
      int peekByte = RS485Serial.peek();
      unsigned char firstByte = (unsigned char)peekByte;
      Serial.print(F("[RS485] Raw bytes available: "));
      Serial.print(available);
      Serial.print(F(" (first byte: 0x"));
      if (firstByte < 0x10) Serial.print('0');
      Serial.print(firstByte, HEX);
      Serial.print(F("="));
      Serial.print((int)firstByte);
      Serial.print(F("='"));
      if (firstByte >= 32 && firstByte < 127) {
        Serial.print((char)firstByte);
      } else if (firstByte == '\r') {
        Serial.print(F("\\r"));
      } else if (firstByte == '\n') {
        Serial.print(F("\\n"));
      } else {
        Serial.print('.');
      }
      Serial.println(F("')"));
    } else {
      // No data available - check if MAX485 might not be powered
      // Only print warning every 10 seconds to avoid spam
      static unsigned long lastPowerWarning = 0;
      if (currentTime - lastPowerWarning > 10000) {
        lastPowerWarning = currentTime;
        Serial.println(F("[RS485] No data received - check MAX485 power (VCC→5V, GND→GND)"));
      }
    }
  }
  
  // Check if command received from RS485
  if (RS485Serial.available()) {
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
      // Valid command received - blink RX LED
      Serial.print(F("[RS485->] Received command: \""));
      Serial.print(commandBuffer);
      Serial.print(F("\" (len="));
      Serial.print(strlen(commandBuffer));
      Serial.print(F(", hex: "));
      for (int i = 0; i < strlen(commandBuffer) && i < 20; i++) {
        unsigned char byteVal = (unsigned char)commandBuffer[i];
        if (byteVal < 0x10) Serial.print('0');
        Serial.print(byteVal, HEX);
        Serial.print(' ');
      }
      Serial.println(F(")"));
      
      // Check if it's a ping command
      if (strcmp(commandBuffer, "PNG") == 0 || strcmp(commandBuffer, "PING") == 0 || 
          strcmp(commandBuffer, "png") == 0 || strcmp(commandBuffer, "ping") == 0) {
        Serial.println(F("[PING] *** PING COMMAND DETECTED ***"));
      }
      
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
      waitingForDWMResponse = false;
      
      if (responseReceived) {
        // Valid response received - send to orchestrator
        Serial.println(F("[DWM->] =========================================="));
        Serial.print(F("[DWM->] ✅ Response received: \""));
        Serial.print(responseBuffer);
        Serial.print(F("\" (len="));
        Serial.print(strlen(responseBuffer));
        Serial.println(F(")"));
        Serial.println(F("[DWM->] =========================================="));
        blink_tx_led();
        
        Serial.print(F("[->RS485] Sending response to orchestrator: \""));
        Serial.print(responseBuffer);
        Serial.println(F("\""));
        rs485_send_response(responseBuffer);
        Serial.println(F("[RS485] Response sent successfully"));
        
        // Clear error LED if it was on
        if (!handshakeComplete) {
          handshakeComplete = true;
          set_error_led(false);
          Serial.println(F("[STATUS] Handshake recovered - Error LED cleared"));
        }
      } else {
        // No response or timeout - send error with detailed diagnostics
        Serial.println(F("[ERROR] =========================================="));
        Serial.println(F("[ERROR] ❌ DWM3001CDK timeout - no response received"));
        Serial.println(F("[ERROR] =========================================="));
        Serial.println(F("[ERROR] Diagnostics:"));
        Serial.print(F("[ERROR]   Command sent: \""));
        Serial.print(commandBuffer);
        Serial.println(F("\""));
        Serial.print(F("[ERROR]   Timeout: "));
        Serial.print(DWM_RESPONSE_TIMEOUT_MS);
        Serial.println(F("ms"));
        Serial.print(F("[ERROR]   DWM serial listening: "));
        Serial.println(DWMSerial.isListening() ? "YES" : "NO");
        Serial.print(F("[ERROR]   DWM serial available: "));
        Serial.println(DWMSerial.available());
        Serial.println(F("[ERROR]"));
        Serial.println(F("[ERROR] Possible causes:"));
        Serial.println(F("[ERROR]   1. DWM3001CDK not powered on"));
        Serial.println(F("[ERROR]   2. Wiring issue (GPIO14→D9, GPIO15→D8, GND→GND)"));
        Serial.println(F("[ERROR]   3. DWM3001CDK firmware not running"));
        Serial.println(F("[ERROR]   4. Baud rate mismatch (should be 115200)"));
        Serial.println(F("[ERROR]   5. DWM3001CDK UART not initialized"));
        Serial.println(F("[ERROR]   6. DWM3001CDK in sleep/reset state"));
        Serial.println(F("[ERROR] =========================================="));
        send_error("DWM_NO_RESPONSE");
      }
    } else {
      Serial.println(F("[WARN] Failed to receive valid command from RS485"));
    }
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
  
  Serial.println(F("[RS485] Starting command reception..."));
  
  while (index < maxLen - 1) {
    if (RS485Serial.available()) {
      int byteRead = RS485Serial.read();
      if (byteRead == -1) {
        // No data available (shouldn't happen after available() check, but handle it)
        continue;
      }
      unsigned char c = (unsigned char)byteRead;  // Cast to unsigned to avoid sign extension issues
      rs485BytesReceived++;
      
      // Log first few characters for debugging
      if (firstChar) {
        Serial.print(F("[RS485] First char received: 0x"));
        if (c < 0x10) Serial.print('0');
        Serial.print(c, HEX);
        Serial.print(F(" ('"));
        if (c >= 32 && c < 127) {
          Serial.print(c);
        } else if (c == '\r') {
          Serial.print(F("\\r"));
        } else if (c == '\n') {
          Serial.print(F("\\n"));
        } else {
          Serial.print('.');
        }
        Serial.println(F("')"));
        
        // Detect baud rate mismatch - corrupted bytes usually have high values or are 0xFF
        if (c == 0xFF || c > 0x7F) {
          Serial.println(F("[ERROR] *** BAUD RATE MISMATCH DETECTED ***"));
          Serial.println(F("[ERROR] Received corrupted byte (0xFF or >0x7F)"));
          Serial.print(F("[ERROR] Arduino is set to: "));
          Serial.print(RS485_BAUD_RATE);
          Serial.println(F(" baud"));
          Serial.println(F("[ERROR] Check orchestrator baud rate setting!"));
          Serial.println(F("[ERROR] Expected: 57600, but might be sending at 115200"));
        }
        firstChar = false;
      }
      
      // Check for command terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {  // Only accept if we have data
          buffer[index] = '\0';  // Null terminate
          Serial.print(F("[RS485] Command complete, received terminator: "));
          Serial.println(c == '\r' ? "\\r" : "\\n");
          return true;
        }
        // Skip leading/empty terminators
        Serial.println(F("[RS485] Skipping leading terminator"));
      } else {
        buffer[index++] = c;
        // Log each character for first few bytes (helpful for ping debugging)
        if (index <= 10) {
          Serial.print(F("[RS485] Char "));
          Serial.print(index);
          Serial.print(F(": 0x"));
          if (c < 0x10) Serial.print('0');
          Serial.print(c, HEX);
          Serial.print(F(" ("));
          Serial.print((int)c);  // Show decimal value too
          Serial.print(F("='"));
          if (c >= 32 && c < 127) {
            Serial.print((char)c);
          } else if (c == '\r') {
            Serial.print(F("\\r"));
          } else if (c == '\n') {
            Serial.print(F("\\n"));
          } else {
            Serial.print('.');
          }
          Serial.println(F("')"));
          
          // Detect baud rate mismatch patterns
          // PNG should be: 0x50 ('P'), 0x4E ('N'), 0x47 ('G')
          // If we see high values (0xE0-0xFF) or unexpected patterns, it's likely a baud rate issue
          if (c == 0xFF || (c >= 0xE0 && c <= 0xFF)) {
            Serial.println(F("[ERROR] *** BAUD RATE MISMATCH DETECTED ***"));
            Serial.print(F("[ERROR] Received corrupted byte: 0x"));
            if (c < 0x10) Serial.print('0');
            Serial.print(c, HEX);
            Serial.print(F(" ("));
            Serial.print((int)c);
            Serial.println(F(")"));
            Serial.print(F("[ERROR] Arduino is receiving at: "));
            Serial.print(RS485_BAUD_RATE);
            Serial.println(F(" baud"));
            Serial.println(F("[ERROR] Orchestrator might be sending at 115200 baud!"));
            Serial.println(F("[ERROR] Fix: Set orchestrator baudrate to 57600"));
          }
          
          // Check if we're receiving PNG pattern
          if (index == 1 && c == 0x50) {
            Serial.println(F("[INFO] Detected 'P' - might be PNG command"));
          } else if (index == 2 && c == 0x4E && buffer[0] == 0x50) {
            Serial.println(F("[INFO] Detected 'PN' - likely PNG command"));
          } else if (index == 3 && c == 0x47 && buffer[0] == 0x50 && buffer[1] == 0x4E) {
            Serial.println(F("[INFO] Detected 'PNG' - PING command received!"));
          }
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
  
  set_rs485_rx_mode();
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
  // #region agent log
  Serial.print(F("[DEBUG] dwm_send_command ENTRY: command=\""));
  Serial.print(command);
  Serial.print(F("\", len="));
  Serial.print(strlen(command));
  Serial.print(F(", DWMSerial.isListening()="));
  Serial.println(DWMSerial.isListening() ? "YES" : "NO");
  // #endregion
  
  // CRITICAL FIX: Ensure DWM serial is listening and verify it stays listening
  if (!DWMSerial.isListening()) {
    // #region agent log
    Serial.println(F("[DEBUG] Switching listener to DWM serial"));
    // #endregion
    DWMSerial.listen();
    delay(20);  // Increased delay to ensure listener switch completes
    
    // Verify the switch was successful
    if (!DWMSerial.isListening()) {
      // #region agent log
      Serial.println(F("[DEBUG] ERROR: Listener switch failed!"));
      // #endregion
      Serial.println(F("[ERROR] Failed to switch to DWM serial listener!"));
      // Try one more time
      DWMSerial.listen();
      delay(20);
    }
  }
  
  // #region agent log
  Serial.print(F("[DEBUG] BEFORE send: command=\""));
  Serial.print(command);
  Serial.println(F("\""));
  Serial.print(F("[DEBUG] Sending bytes: "));
  for (int i = 0; i < strlen(command); i++) {
    Serial.print(F("0x"));
    if ((unsigned char)command[i] < 0x10) Serial.print('0');
    Serial.print((unsigned char)command[i], HEX);
    Serial.print(' ');
  }
  Serial.println(F("0x0D 0x0A (\\r\\n)"));
  // #endregion
  
  // Send command exactly as orchestrator sent it
  // Orchestrator sends: command + \r\n
  // We forward: command + \r\n (identical format)
  
  // #region agent log
  Serial.print(F("[DEBUG] About to send to DWMSerial, available before: "));
  Serial.println(DWMSerial.available());
  // #endregion
  
  DWMSerial.print(command);
  DWMSerial.print("\r\n");  // Add terminator (same as orchestrator)
  
  // #region agent log
  Serial.println(F("[DEBUG] AFTER send: command sent to DWMSerial"));
  Serial.print(F("[DEBUG] Available after send: "));
  Serial.println(DWMSerial.available());
  // #endregion
  
  // CRITICAL FIX: Increased delay to ensure transmission completes
  // SoftwareSerial at 115200 baud needs more time:
  // - Each byte takes ~87us at 115200 baud
  // - For "PNG\r\n" (5 bytes) = ~435us minimum
  // - Add margin for SoftwareSerial overhead and DWM processing time
  // - Also need time for DWM to process command and start responding
  // - SoftwareSerial needs additional time to complete bit transmission
  delay(50);  // Increased to 50ms to ensure reliable transmission and DWM processing
  
  // #region agent log
  // Check if any data appeared immediately (unlikely but possible)
  int immediateAvailable = DWMSerial.available();
  if (immediateAvailable > 0) {
    Serial.print(F("[DEBUG] Data already available after send delay: "));
    Serial.print(immediateAvailable);
    Serial.println(F(" bytes"));
  } else {
    Serial.println(F("[DEBUG] No immediate data, will wait for response"));
  }
  // #endregion
  
  // #region agent log
  Serial.println(F("[DEBUG] dwm_send_command EXIT"));
  // #endregion
}

/**
 * Receive response from DWM3001CDK
 * Reads until \r or \n is received
 * Returns true if valid response received, false on timeout
 */
bool dwm_receive_response(char* buffer, int maxLen) {
  // #region agent log
  Serial.println(F("[DEBUG] dwm_receive_response ENTRY"));
  Serial.flush();  // CRITICAL: Force output to appear immediately
  Serial.print(F("[DEBUG] maxLen="));
  Serial.print(maxLen);
  Serial.print(F(", DWMSerial.isListening()="));
  Serial.println(DWMSerial.isListening() ? "YES" : "NO");
  Serial.flush();
  // #endregion
  
  int index = 0;
  unsigned long startTime = millis();
  bool firstChar = true;
  int bytesReceived = 0;
  
  // #region agent log
  Serial.println(F("[DEBUG] About to print waiting message..."));
  Serial.flush();
  // #endregion
  
  Serial.print(F("[DWM RX] Waiting for response (timeout="));
  Serial.print(DWM_RESPONSE_TIMEOUT_MS);
  Serial.println(F("ms)..."));
  Serial.flush();  // CRITICAL: Force output
  
  // CRITICAL FIX: Ensure DWM serial is listening and STAYS listening
  // Don't allow listener to switch away during response wait
  // #region agent log
  Serial.println(F("[DEBUG] Checking listener state..."));
  Serial.flush();
  // #endregion
  
  if (!DWMSerial.isListening()) {
    // #region agent log
    Serial.println(F("[DEBUG] DWM serial not listening, switching..."));
    Serial.flush();
    // #endregion
    Serial.println(F("[DWM RX] WARNING: DWM serial not listening! Switching..."));
    Serial.flush();
    DWMSerial.listen();
    delay(20);  // Increased delay to ensure listener switch completes
  }
  
  // #region agent log
  Serial.print(F("[DEBUG] Verified listener state: DWMSerial.isListening()="));
  Serial.println(DWMSerial.isListening() ? "YES" : "NO");
  Serial.flush();
  // #endregion
  
  // CRITICAL FIX: Small delay after ensuring listener is set
  // This allows SoftwareSerial to stabilize before we start reading
  // #region agent log
  Serial.println(F("[DEBUG] Waiting 10ms for SoftwareSerial stabilization..."));
  Serial.flush();
  // #endregion
  delay(10);
  
  // Check initial availability
  // #region agent log
  Serial.println(F("[DEBUG] Checking initial available bytes..."));
  Serial.flush();
  // #endregion
  int initialAvailable = DWMSerial.available();
  // #region agent log
  Serial.print(F("[DEBUG] Initial bytes available: "));
  Serial.println(initialAvailable);
  Serial.print(F("[DEBUG] Listener state check: DWMSerial.isListening()="));
  Serial.println(DWMSerial.isListening() ? "YES" : "NO");
  Serial.flush();
  // #endregion
  if (initialAvailable > 0) {
    Serial.print(F("[DWM RX] Already have "));
    Serial.print(initialAvailable);
    Serial.println(F(" bytes available"));
    // #region agent log
    Serial.print(F("[DEBUG] Reading initial bytes: "));
    char peekChar = DWMSerial.peek();
    Serial.print(F("0x"));
    if ((unsigned char)peekChar < 0x10) Serial.print('0');
    Serial.print((unsigned char)peekChar, HEX);
    Serial.print(F(" ('"));
    if (peekChar >= 32 && peekChar < 127) Serial.print(peekChar);
    Serial.println(F("')"));
    // #endregion
  }
  
  unsigned long lastByteTime = startTime;  // Track when we last received a byte
  unsigned long lastAvailableCheck = startTime;
  int consecutiveNoDataChecks = 0;
  
  // #region agent log
  Serial.println(F("[DEBUG] Entering main receive loop..."));
  Serial.flush();
  // #endregion
  
  // CRITICAL DIAGNOSTIC: Before entering the main loop, do an aggressive check
  // to see if ANY bytes are arriving at all, even if they're not valid responses
  // This helps diagnose if the issue is:
  // 1. No data arriving (hardware/wiring issue)
  // 2. Data arriving but not being read (SoftwareSerial issue)
  // 3. Data arriving but wrong format (firmware/baud rate issue)
  // #region agent log
  Serial.println(F("[DEBUG] Performing aggressive byte detection check..."));
  Serial.flush();
  
  // Check multiple times over 200ms to catch any delayed responses
  int maxAggressiveChecks = 20;
  int aggressiveBytesFound = 0;
  for (int check = 0; check < maxAggressiveChecks; check++) {
    delay(10);  // Check every 10ms
    int availableNow = DWMSerial.available();
    if (availableNow > 0) {
      aggressiveBytesFound = availableNow;
      Serial.print(F("[DEBUG] AGGRESSIVE CHECK #"));
      Serial.print(check);
      Serial.print(F(": Found "));
      Serial.print(availableNow);
      Serial.print(F(" bytes available at "));
      Serial.print(check * 10);
      Serial.println(F("ms!"));
      Serial.flush();
      break;  // Found bytes, stop checking
    }
  }
  
  if (aggressiveBytesFound > 0) {
    Serial.print(F("[DEBUG] Reading "));
    Serial.print(aggressiveBytesFound);
    Serial.println(F(" bytes from aggressive check..."));
    Serial.flush();
    for (int i = 0; i < aggressiveBytesFound && i < 20; i++) {
      char c = DWMSerial.read();
      Serial.print(F("[DEBUG] Byte "));
      Serial.print(i);
      Serial.print(F(": 0x"));
      if ((unsigned char)c < 0x10) Serial.print('0');
      Serial.print((unsigned char)c, HEX);
      Serial.print(F(" ("));
      Serial.print((int)c);
      Serial.print(F(") '"));
      if (c >= 32 && c < 127) Serial.print(c);
      Serial.println(F("'"));
      Serial.flush();
    }
  } else {
    Serial.print(F("[DEBUG] AGGRESSIVE CHECK: No bytes available after "));
    Serial.print(maxAggressiveChecks * 10);
    Serial.println(F("ms of checking"));
    Serial.println(F("[DEBUG] This indicates DWM is NOT sending any data"));
    Serial.println(F("[DEBUG] Possible causes:"));
    Serial.println(F("[DEBUG]   1. DWM not powered"));
    Serial.println(F("[DEBUG]   2. Wiring incorrect (GPIO14→D9, GPIO15→D8)"));
    Serial.println(F("[DEBUG]   3. DWM firmware not running"));
    Serial.println(F("[DEBUG]   4. Baud rate mismatch"));
    Serial.flush();
  }
  // #endregion
  
  while (index < maxLen - 1) {
    // CRITICAL FIX: Periodically verify listener hasn't switched away
    // This prevents missing data if listener gets switched by main loop
    if (!DWMSerial.isListening() && waitingForDWMResponse) {
      // #region agent log
      Serial.println(F("[DEBUG] WARNING: Listener switched away during receive! Restoring..."));
      // #endregion
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
      
      // #region agent log
      if (firstChar) {
        firstChar = false;  // Mark that we've received the first byte
        Serial.print(F("[DEBUG] FIRST BYTE received: 0x"));
        if ((unsigned char)c < 0x10) Serial.print('0');
        Serial.print((unsigned char)c, HEX);
        Serial.print(F(" ("));
        Serial.print((int)c);
        Serial.print(F(") '"));
        if (c >= 32 && c < 127) Serial.print(c);
        Serial.print(F("' at elapsed="));
        Serial.print(millis() - startTime);
        Serial.println(F("ms"));
        Serial.flush();
        
        // Also check if more bytes are already available (DWM might send fast)
        int moreAvailable = DWMSerial.available();
        if (moreAvailable > 0) {
          Serial.print(F("[DEBUG] More bytes already available: "));
          Serial.print(moreAvailable);
          Serial.println(F(" (DWM responding quickly)"));
          Serial.flush();
        }
      }
      // #endregion
      
      // Log first few characters for debugging
      if (firstChar) {
        Serial.print(F("[DWM RX] First char: 0x"));
        if ((unsigned char)c < 0x10) Serial.print('0');
        Serial.print((unsigned char)c, HEX);
        Serial.print(F(" ('"));
        if (c >= 32 && c < 127) {
          Serial.print(c);
        } else if (c == '\r') {
          Serial.print(F("\\r"));
        } else if (c == '\n') {
          Serial.print(F("\\n"));
        } else {
          Serial.print('.');
        }
        Serial.println(F("')"));
        firstChar = false;
      }
      
      // CRITICAL FIX: Less aggressive filtering - accept more characters
      // Some DWM responses might have control characters we need to handle
      // Only filter truly invalid characters (null bytes)
      if (c == 0x00) {
        // #region agent log
        Serial.println(F("[DEBUG] Filtered null byte"));
        // #endregion
        Serial.print(F("[DWM RX] Filtered null byte: 0x00"));
        Serial.println();
        continue;  // Skip null bytes only
      }
      
      // Log non-printable characters but don't filter them (they might be valid)
      if (c < 32 && c != '\r' && c != '\n' && c != '\t') {
        // #region agent log
        Serial.print(F("[DEBUG] Non-printable char received: 0x"));
        if ((unsigned char)c < 0x10) Serial.print('0');
        Serial.print((unsigned char)c, HEX);
        Serial.print(F(" ("));
        Serial.print((int)c);
        Serial.println(F(") - accepting anyway"));
        // #endregion
        // Don't filter - accept it and see what happens
      }
      
      // Check for response terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {  // Only accept if we have data
          buffer[index] = '\0';  // Null terminate
          // #region agent log
          Serial.print(F("[DEBUG] RESPONSE COMPLETE: \""));
          Serial.print(buffer);
          Serial.print(F("\", len="));
          Serial.print(index);
          Serial.print(F(", bytesReceived="));
          Serial.println(bytesReceived);
          // #endregion
          Serial.print(F("[DWM RX] Response complete: \""));
          Serial.print(buffer);
          Serial.print(F("\" ("));
          Serial.print(index);
          Serial.println(F(" chars)"));
          return true;
        }
        // Empty line, continue reading
        Serial.println(F("[DWM RX] Empty line, continuing..."));
      } else {
        buffer[index++] = c;
      }
      
      lastByteTime = millis();  // Track when we last received a byte
      startTime = millis();  // Reset timeout on each character
      
      // #region agent log
      // Log every 10th byte to track progress without spamming
      if (bytesReceived % 10 == 0) {
        Serial.print(F("[DEBUG] Received "));
        Serial.print(bytesReceived);
        Serial.print(F(" bytes so far, index="));
        Serial.print(index);
        Serial.print(F(", last char: 0x"));
        if ((unsigned char)c < 0x10) Serial.print('0');
        Serial.print((unsigned char)c, HEX);
        Serial.println();
      }
      // #endregion
    } else {
      // CRITICAL FIX: Small delay when no data available
      // This prevents tight looping and allows SoftwareSerial interrupt handlers to run
      // SoftwareSerial uses interrupts, so we need to yield CPU time
      delay(1);  // 1ms delay allows interrupt handlers to process incoming bytes
    }
    
    // Timeout check
    unsigned long elapsed = millis() - startTime;
    // #region agent log
    if (elapsed > 1000 && elapsed % 1000 == 0) {
      Serial.print(F("[DEBUG] Still waiting, elapsed="));
      Serial.print(elapsed);
      Serial.print(F("ms, bytesReceived="));
      Serial.print(bytesReceived);
      Serial.print(F(", index="));
      Serial.print(index);
      Serial.print(F(", available="));
      Serial.println(DWMSerial.available());
    }
    // #endregion
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
      // #region agent log
      Serial.print(F("[DEBUG] TIMEOUT: elapsed="));
      Serial.print(elapsed);
      Serial.print(F("ms, bytesReceived="));
      Serial.print(bytesReceived);
      Serial.print(F(", index="));
      Serial.print(index);
      Serial.print(F(", buffer so far: \""));
      if (index > 0) {
        buffer[index] = '\0';
        Serial.print(buffer);
      }
      Serial.println(F("\""));
      // #endregion
      
      Serial.print(F("[WARN] DWM response timeout - no data received (checked for "));
      Serial.print(elapsed);
      Serial.print(F("ms, bytes seen: "));
      Serial.print(bytesReceived);
      Serial.println(F(")"));
      
      // Diagnostic: Check if DWM serial is still available
      int stillAvailable = DWMSerial.available();
      // #region agent log
      Serial.print(F("[DEBUG] Still available bytes: "));
      Serial.println(stillAvailable);
      // #endregion
      if (stillAvailable > 0) {
        Serial.print(F("[WARN] DWM serial still has "));
        Serial.print(stillAvailable);
        Serial.println(F(" bytes available - possible parsing issue"));
        Serial.println(F("[WARN] Attempting to read remaining bytes..."));
        char tempBuffer[64];
        int tempIndex = 0;
        while (DWMSerial.available() && tempIndex < sizeof(tempBuffer) - 1) {
          tempBuffer[tempIndex++] = DWMSerial.read();
        }
        tempBuffer[tempIndex] = '\0';
        Serial.print(F("[WARN] Remaining data: \""));
        Serial.print(tempBuffer);
        Serial.println(F("\""));
      }
      
      // CRITICAL FIX: Before giving up, try one more aggressive read attempt
      // Sometimes SoftwareSerial has bytes but available() doesn't report them correctly
      Serial.println(F("[WARN] Attempting final aggressive read..."));
      delay(100);  // Give it more time
      
      // Try reading any remaining bytes aggressively
      int finalAttemptBytes = 0;
      char finalBuffer[64] = {0};
      int finalIndex = 0;
      unsigned long finalStart = millis();
      
      while (finalIndex < sizeof(finalBuffer) - 1 && (millis() - finalStart < 500)) {
        if (DWMSerial.available()) {
          char fc = DWMSerial.read();
          finalBuffer[finalIndex++] = fc;
          finalAttemptBytes++;
        } else {
          delay(1);  // Small delay between checks
        }
      }
      
      if (finalAttemptBytes > 0) {
        finalBuffer[finalIndex] = '\0';
        // #region agent log
        Serial.print(F("[DEBUG] Final aggressive read got "));
        Serial.print(finalAttemptBytes);
        Serial.print(F(" bytes: \""));
        for (int i = 0; i < finalIndex; i++) {
          if (finalBuffer[i] >= 32 && finalBuffer[i] < 127) {
            Serial.print(finalBuffer[i]);
          } else {
            Serial.print(F("\\x"));
            if ((unsigned char)finalBuffer[i] < 0x10) Serial.print('0');
            Serial.print((unsigned char)finalBuffer[i], HEX);
          }
        }
        Serial.println(F("\""));
        // #endregion
        Serial.print(F("[WARN] Final read attempt recovered "));
        Serial.print(finalAttemptBytes);
        Serial.print(F(" bytes: \""));
        Serial.print(finalBuffer);
        Serial.println(F("\""));
        
        // If this looks like a valid response, use it
        if (strncmp(finalBuffer, "OK", 2) == 0 || strncmp(finalBuffer, "ERR", 3) == 0) {
          strncpy(buffer, finalBuffer, maxLen - 1);
          buffer[maxLen - 1] = '\0';
          // #region agent log
          Serial.println(F("[DEBUG] Using recovered response from final read"));
          // #endregion
          return true;
        }
      }
      
      // Additional diagnostic: Check if DWM3001CDK might be sending but we're not receiving
      Serial.println(F("[WARN] =========================================="));
      Serial.println(F("[WARN] DWM3001CDK Communication Failure"));
      Serial.println(F("[WARN] =========================================="));
      Serial.println(F("[WARN] Check Arduino Serial Monitor (USB) for:"));
      Serial.println(F("[WARN]   - Did startup messages appear?"));
      Serial.println(F("[WARN]   - Any data from DWM3001CDK at all?"));
      Serial.println(F("[WARN]"));
      Serial.println(F("[WARN] If NO startup messages appeared:"));
      Serial.println(F("[WARN]   1. DWM3001CDK firmware not running"));
      Serial.println(F("[WARN]   2. Wiring incorrect (check GPIO14→D9, GPIO15→D8)"));
      Serial.println(F("[WARN]   3. DWM3001CDK not powered"));
      Serial.println(F("[WARN]   4. Baud rate mismatch"));
      Serial.println(F("[WARN]"));
      Serial.println(F("[WARN] If startup messages DID appear but commands fail:"));
      Serial.println(F("[WARN]   1. DWM3001CDK might be in wrong state"));
      Serial.println(F("[WARN]   2. Command format might be wrong"));
      Serial.println(F("[WARN]   3. DWM3001CDK UART might have issues"));
      Serial.println(F("[WARN]   4. SoftwareSerial at 115200 baud may be unreliable"));
      Serial.println(F("[WARN] =========================================="));
      
      // #region agent log
      Serial.println(F("[DEBUG] dwm_receive_response EXIT: false (timeout)"));
      // #endregion
      return false;  // No response
    }
  }
  
  // Buffer full
  buffer[maxLen - 1] = '\0';
  // #region agent log
  Serial.print(F("[DEBUG] BUFFER FULL: index="));
  Serial.print(index);
  Serial.print(F(", maxLen="));
  Serial.println(maxLen);
  // #endregion
  Serial.print(F("[ERROR] DWM response buffer full ("));
  Serial.print(index);
  Serial.println(F(" chars)"));
  // #region agent log
  Serial.println(F("[DEBUG] dwm_receive_response EXIT: false (buffer full)"));
  // #endregion
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
    
    // #region agent log
    Serial.print(F("[DEBUG] After handshake send, available: "));
    Serial.println(DWMSerial.available());
    // #endregion
    
    // Wait for response
    Serial.println(F("[HANDSHAKE] Waiting for response..."));
    bool handshakeResponse = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
    
    // Clear the flag after response
    waitingForDWMResponse = false;
    
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
    
    // #region agent log
    Serial.println(F("[DEBUG] test_dwm_communication: About to send test command"));
    Serial.flush();
    // #endregion
    
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
    // #region agent log
    Serial.println(F("[DEBUG] About to call dwm_send_command..."));
    Serial.flush();
    // #endregion
    dwm_send_command(testCmd);
    
    // #region agent log
    Serial.println(F("[DEBUG] dwm_send_command returned, about to call dwm_receive_response..."));
    Serial.flush();
    // #endregion
    
    // Wait for response
    bool responseReceived = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
    
    // #region agent log
    Serial.print(F("[DEBUG] dwm_receive_response returned: "));
    Serial.println(responseReceived ? "true" : "false");
    Serial.flush();
    // #endregion
    
    // Clear the flag after response
    waitingForDWMResponse = false;
    
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
