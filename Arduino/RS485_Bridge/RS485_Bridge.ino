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
#define DWM_RX_PIN 8          // Arduino RX from DWM TX (GPIO14/TXD0)
#define DWM_TX_PIN 9          // Arduino TX to DWM RX (GPIO15/RXD0)

// LED Indicator Pins
#define LED_RX_PIN 4          // RX Activity LED
#define LED_TX_PIN 5          // TX Activity LED
#define LED_ERROR_PIN 6       // Error LED
#define LED_HEARTBEAT_PIN 13  // Heartbeat LED (built-in)

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

#define BAUD_RATE 57600               // Reduced from 115200 for SoftwareSerial reliability
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

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  // Initialize Serial for debugging (USB serial port)
  Serial.begin(115200);
  
  // Wait for serial port to stabilize and clear any garbage data
  // This prevents garbled characters at startup
  while (!Serial) {
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
  RS485Serial.begin(BAUD_RATE);
  DWMSerial.begin(BAUD_RATE);
  Serial.print(F("[SETUP] Serial ports initialized at "));
  Serial.print(BAUD_RATE);
  Serial.println(F(" baud"));
  
  // Now set RS485 to receive mode (safe to flush now)
  set_rs485_rx_mode();
  Serial.println(F("[SETUP] RS485 set to RX mode"));
  
  // Small delay for serial ports to stabilize
  delay(100);
  
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
  Serial.println(F("[SETUP] Initialization complete - entering main loop"));
  Serial.println(F("==========================================\n"));
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Ensure we are listening to RS485 for incoming commands
  if (!RS485Serial.isListening()) {
    RS485Serial.listen();
  }
  
  // Update heartbeat LED
  update_heartbeat();
  
  // Update activity LEDs (turn off after blink duration)
  update_activity_leds();
  
  // Check if command received from RS485
  if (RS485Serial.available()) {
    // Show activity detected and the first byte in hex to help diagnose baud/wiring issues
    Serial.print(F("[RS485] Activity detected (0x"));
    if (RS485Serial.peek() < 0x10) Serial.print('0');
    Serial.print(RS485Serial.peek(), HEX);
    Serial.println(F("), receiving command..."));
    
    if (rs485_receive_command(commandBuffer, COMMAND_BUFFER_SIZE)) {
      // Valid command received - blink RX LED
      Serial.print(F("[RS485->] Received: "));
      Serial.println(commandBuffer);
      blink_rx_led();
      
      // Forward command to DWM3001CDK
      Serial.print(F("[->DWM] Sending: "));
      Serial.println(commandBuffer);
      DWMSerial.listen(); // Switch listener to DWM before sending to catch immediate response
      dwm_send_command(commandBuffer);
      
      // Wait for response from DWM3001CDK
      if (dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE)) {
        // Valid response received - send to orchestrator
        Serial.print(F("[DWM->] Response: "));
        Serial.println(responseBuffer);
        blink_tx_led();
        rs485_send_response(responseBuffer);
        Serial.print(F("[->RS485] Sent: "));
        Serial.println(responseBuffer);
        
        // Clear error LED if it was on
        if (!handshakeComplete) {
          handshakeComplete = true;
          set_error_led(false);
          Serial.println(F("[STATUS] Handshake recovered - Error LED cleared"));
        }
      } else {
        // No response or timeout - send error
        Serial.println(F("[ERROR] DWM3001CDK timeout - no response received"));
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
 * DE=HIGH, RE=HIGH
 */
void set_rs485_tx_mode() {
  digitalWrite(RS485_DE_PIN, HIGH);
  digitalWrite(RS485_RE_PIN, HIGH);
  delayMicroseconds(RS485_TX_DELAY_US);  // Wait for transceiver to switch
}

/**
 * Set RS485 transceiver to receive mode
 * DE=LOW, RE=LOW
 */
void set_rs485_rx_mode() {
  // No flush needed for SoftwareSerial as print() is blocking
  // RS485Serial.flush(); 
  delayMicroseconds(RS485_TX_DELAY_US);  // Wait for last bit to transmit
  digitalWrite(RS485_DE_PIN, LOW);
  digitalWrite(RS485_RE_PIN, LOW);
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
  
  while (index < maxLen - 1) {
    if (RS485Serial.available()) {
      char c = RS485Serial.read();
      
      // Check for command terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {  // Only accept if we have data
          buffer[index] = '\0';  // Null terminate
          return true;
        }
        // Skip leading/empty terminators
      } else {
        buffer[index++] = c;
      }
      
      startTime = millis();  // Reset timeout on each character
    }
    
    // Timeout check (1 second for command reception)
    if (millis() - startTime > 1000) {
      if (index > 0) {
        buffer[index] = '\0';
        Serial.print(F("[WARN] RS485 command timeout, partial: "));
        Serial.println(buffer);
        return true;  // Return partial command
      }
      Serial.println(F("[WARN] RS485 command receive timeout"));
      return false;
    }
  }
  
  // Buffer full
  buffer[maxLen - 1] = '\0';
  Serial.println(F("[ERROR] RS485 command buffer full"));
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
void dwm_send_command(const char* command) {
  DWMSerial.print(command);
  DWMSerial.print("\r\n");  // Always add terminators
  // DWMSerial.flush(); // Removed: would clear RX buffer immediately after sending
}

/**
 * Receive response from DWM3001CDK
 * Reads until \r or \n is received
 * Returns true if valid response received, false on timeout
 */
bool dwm_receive_response(char* buffer, int maxLen) {
  int index = 0;
  unsigned long startTime = millis();
  
  while (index < maxLen - 1) {
    if (DWMSerial.available()) {
      char c = DWMSerial.read();
      
      // Check for response terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {  // Only accept if we have data
          buffer[index] = '\0';  // Null terminate
          return true;
        }
        // Empty line, continue reading
      } else {
        buffer[index++] = c;
      }
      
      startTime = millis();  // Reset timeout on each character
    }
    
    // Timeout check
    if (millis() - startTime > DWM_RESPONSE_TIMEOUT_MS) {
      if (index > 0) {
        buffer[index] = '\0';
        Serial.print(F("[WARN] DWM response timeout, partial: "));
        Serial.println(buffer);
        return true;  // Return partial response
      }
      Serial.println(F("[WARN] DWM response timeout - no data received"));
      return false;  // No response
    }
  }
  
  // Buffer full
  buffer[maxLen - 1] = '\0';
  Serial.println(F("[ERROR] DWM response buffer full"));
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
    Serial.println(F(": Sending NT command..."));
    
    // Send Node Type query
    DWMSerial.listen();
    dwm_send_command("NT");
    
    // Wait for response
    if (dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE)) {
      Serial.print(F("[HANDSHAKE] Received: "));
      Serial.println(responseBuffer);
      
      // Check if response is valid (should be "OK TX_V2" or "OK RX_V2")
      if (strncmp(responseBuffer, "OK ", 3) == 0) {
        // Extract node type
        strncpy(nodeType, responseBuffer + 3, sizeof(nodeType) - 1);
        nodeType[sizeof(nodeType) - 1] = '\0';
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
