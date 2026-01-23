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
 *     - D8  = RX (from DWM3001CDK GPIO27/TXD0)
 *     - D9  = TX (to DWM3001CDK GPIO15/RXD0)
 *   
 *   LED Indicators:
 *     - D4  = RX Activity (blinks when receiving from orchestrator)
 *     - D5  = TX Activity (blinks when transmitting to orchestrator)
 *     - D6  = Error LED (solid during errors)
 *     - D13 = Heartbeat (1Hz blink to show Arduino is alive)
 * 
 * Communication Protocol:
 *   - RS485 Baud Rate: 115200 (orchestrator to Arduino)
 *   - DWM Baud Rate: 57600 (Arduino to DWM3001CDK)
 *   - Command terminator: \r\n
 *   - Response format: OK <data>\r\n or ERR_<type>\r\n
 * 
 * Author: Generated for INVERITA DWM3001C Test Rig
 * Version: 2.0 - Stable RX/TX
 * Date: 2026-01-23
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
#define DWM_RX_PIN 8          // Arduino RX from DWM TX (GPIO27)
#define DWM_TX_PIN 9          // Arduino TX to DWM RX (GPIO15)

// LED Indicator Pins
#define LED_RX_PIN 4          // RX Activity LED
#define LED_TX_PIN 5          // TX Activity LED
#define LED_ERROR_PIN 6       // Error LED
#define LED_HEARTBEAT_PIN 13  // Heartbeat LED (built-in)

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

#define RS485_BAUD_RATE 115200        // RS485 baud rate
#define DWM_BAUD_RATE 57600           // DWM3001CDK baud rate
#define RS485_TX_DELAY_US 200         // Delay for RS485 transceiver switching
#define COMMAND_BUFFER_SIZE 256       // Command buffer size
#define DWM_RESPONSE_TIMEOUT_MS 5000  // Timeout for DWM response
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
char nodeType[16] = "UNKNOWN";
unsigned long lastHeartbeatTime = 0;
bool heartbeatState = false;

// Critical: Flag to prevent listener switching during DWM communication
bool waitingForDWMResponse = false;

// LED state tracking
unsigned long ledRxOffTime = 0;
unsigned long ledTxOffTime = 0;
bool ledRxActive = false;
bool ledTxActive = false;

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(100);
  Serial.flush();
  
  Serial.println(F("\n=== RS485 Bridge Firmware v2.0 ==="));
  
  // Initialize LED pins
  pinMode(LED_RX_PIN, OUTPUT);
  pinMode(LED_TX_PIN, OUTPUT);
  pinMode(LED_ERROR_PIN, OUTPUT);
  pinMode(LED_HEARTBEAT_PIN, OUTPUT);
  digitalWrite(LED_RX_PIN, LOW);
  digitalWrite(LED_TX_PIN, LOW);
  digitalWrite(LED_ERROR_PIN, LOW);
  digitalWrite(LED_HEARTBEAT_PIN, LOW);
  
  // Initialize RS485 control pins
  pinMode(RS485_DE_PIN, OUTPUT);
  pinMode(RS485_RE_PIN, OUTPUT);
  
  // Initialize serial ports
  RS485Serial.begin(RS485_BAUD_RATE);
  DWMSerial.begin(DWM_BAUD_RATE);
  
  // Set RS485 to receive mode
  set_rs485_rx_mode();
  
  // Clear any garbage data
  while (RS485Serial.available()) RS485Serial.read();
  while (DWMSerial.available()) DWMSerial.read();
  
  // Startup LED sequence
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_HEARTBEAT_PIN, HIGH);
    delay(100);
    digitalWrite(LED_HEARTBEAT_PIN, LOW);
    delay(100);
  }
  
  // Perform handshake with DWM3001CDK
  Serial.println(F("[SETUP] Starting handshake..."));
  handshakeComplete = perform_handshake();
  
  if (handshakeComplete) {
    Serial.print(F("[SETUP] Handshake SUCCESS - Node Type: "));
    Serial.println(nodeType);
  } else {
    Serial.println(F("[SETUP] Handshake FAILED"));
    set_error_led(true);
  }
  
  // Initialize heartbeat timer
  lastHeartbeatTime = millis();
  
  Serial.println(F("[SETUP] Ready - waiting for commands\n"));
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Update heartbeat LED
  update_heartbeat();
  
  // Update activity LEDs
  update_activity_leds();
  
  // PRIORITY: Check for commands from RS485 (orchestrator)
  if (RS485Serial.available() && !waitingForDWMResponse) {
    // Ensure RS485 listener is active
    if (!RS485Serial.isListening()) {
      RS485Serial.listen();
      delay(5);
    }
    
    // Ensure RS485 is in RX mode
    set_rs485_rx_mode();
    
    if (rs485_receive_command(commandBuffer, COMMAND_BUFFER_SIZE)) {
      Serial.print(F("[RS485->] Command: \""));
      Serial.print(commandBuffer);
      Serial.println(F("\""));
      
      blink_rx_led();
      
      // Forward command to DWM3001CDK
      Serial.print(F("[->DWM] Forwarding: \""));
      Serial.print(commandBuffer);
      Serial.println(F("\""));
      
      // Switch to DWM listener
      if (!DWMSerial.isListening()) {
        DWMSerial.listen();
        delay(10);
      }
      
      // Clear DWM buffer
      while (DWMSerial.available()) DWMSerial.read();
      
      // Mark waiting for response
      waitingForDWMResponse = true;
      
      // Send command to DWM
      dwm_send_command(commandBuffer);
      
      // Wait for response
      bool responseReceived = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
      
      // Clear flag
      waitingForDWMResponse = false;
      
      if (responseReceived) {
        Serial.print(F("[DWM->] Response: \""));
        Serial.print(responseBuffer);
        Serial.println(F("\""));
        
        blink_tx_led();
        
        // Send response to orchestrator
        Serial.print(F("[->RS485] Sending: \""));
        Serial.print(responseBuffer);
        Serial.println(F("\""));
        
        rs485_send_response(responseBuffer);
        
        // Clear error LED if handshake recovered
        if (!handshakeComplete) {
          handshakeComplete = true;
          set_error_led(false);
        }
      } else {
        Serial.println(F("[ERROR] DWM timeout - no response"));
        send_error("DWM_NO_RESPONSE");
      }
      
      // Switch back to RS485 listener for next command
      if (!RS485Serial.isListening()) {
        RS485Serial.listen();
        delay(5);
      }
    }
  }
  
  // Small delay to prevent CPU spinning
  delay(1);
}

// ============================================================================
// RS485 CONTROL FUNCTIONS
// ============================================================================

/**
 * Set RS485 transceiver to transmit mode
 */
void set_rs485_tx_mode() {
  // Wait for any ongoing transmission to complete
  delayMicroseconds(RS485_TX_DELAY_US);
  
  // Set control pins
  digitalWrite(RS485_DE_PIN, HIGH);   // Enable driver
  digitalWrite(RS485_RE_PIN, HIGH);   // Disable receiver
  
  // Wait for transceiver to stabilize
  delayMicroseconds(RS485_TX_DELAY_US);
}

/**
 * Set RS485 transceiver to receive mode
 */
void set_rs485_rx_mode() {
  // Wait for any ongoing transmission to complete
  delayMicroseconds(RS485_TX_DELAY_US);
  
  // Set control pins
  digitalWrite(RS485_DE_PIN, LOW);    // Disable driver
  digitalWrite(RS485_RE_PIN, LOW);     // Enable receiver
  
  // Small delay for stabilization
  delayMicroseconds(50);
}

// ============================================================================
// RS485 COMMUNICATION FUNCTIONS
// ============================================================================

/**
 * Receive command from RS485 (orchestrator)
 */
bool rs485_receive_command(char* buffer, int maxLen) {
  int index = 0;
  unsigned long startTime = millis();
  unsigned long lastByteTime = startTime;
  
  while (index < maxLen - 1) {
    if (RS485Serial.available()) {
      int byteRead = RS485Serial.read();
      if (byteRead == -1) continue;
      
      unsigned char c = (unsigned char)byteRead;
      
      // Check for command terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {
          buffer[index] = '\0';
          return true;
        }
        // Skip empty terminators
        continue;
      }
      
      // Store character
      buffer[index++] = c;
      lastByteTime = millis();
      startTime = millis();  // Reset timeout
    }
    
    // Timeout check (1 second)
    if (millis() - startTime > 1000) {
      if (index > 0) {
        buffer[index] = '\0';
        return true;  // Return partial command
      }
      return false;
    }
    
    // Small delay to prevent tight loop
    delay(1);
  }
  
  // Buffer full
  buffer[maxLen - 1] = '\0';
  return false;
}

/**
 * Send response to RS485 (orchestrator)
 */
void rs485_send_response(const char* response) {
  // Switch to TX mode
  set_rs485_tx_mode();
  
  // Ensure RS485 listener is active
  if (!RS485Serial.isListening()) {
    RS485Serial.listen();
    delay(5);
  }
  
  // Send response
  RS485Serial.print(response);
  
  // Add terminator if not present
  int len = strlen(response);
  if (len == 0 || (response[len-1] != '\n' && response[len-1] != '\r')) {
    RS485Serial.print("\r\n");
  }
  
  // Wait for transmission to complete
  delay(10);
  
  // Switch back to RX mode
  set_rs485_rx_mode();
}

// ============================================================================
// DWM3001CDK COMMUNICATION FUNCTIONS
// ============================================================================

/**
 * Send command to DWM3001CDK
 */
void dwm_send_command(const char* command) {
  // Ensure DWM listener is active
  if (!DWMSerial.isListening()) {
    DWMSerial.listen();
    delay(10);
  }
  
  // Send command with terminator
  DWMSerial.print(command);
  DWMSerial.print("\r\n");
  
  // Wait for transmission to complete
  delay(50);
}

/**
 * Receive response from DWM3001CDK
 */
bool dwm_receive_response(char* buffer, int maxLen) {
  int index = 0;
  unsigned long startTime = millis();
  
  // Ensure DWM listener is active
  if (!DWMSerial.isListening()) {
    DWMSerial.listen();
    delay(10);
  }
  
  // Small stabilization delay
  delay(10);
  
  while (index < maxLen - 1) {
    // Periodically verify listener hasn't switched
    if (!DWMSerial.isListening() && waitingForDWMResponse) {
      DWMSerial.listen();
      delay(10);
    }
    
    if (DWMSerial.available()) {
      char c = DWMSerial.read();
      
      // Filter null bytes only
      if (c == 0x00) continue;
      
      // Check for response terminator
      if (c == '\r' || c == '\n') {
        if (index > 0) {
          buffer[index] = '\0';
          return true;
        }
        // Skip empty lines
        continue;
      }
      
      // Store character
      buffer[index++] = c;
      startTime = millis();  // Reset timeout
    } else {
      // Small delay when no data
      delay(1);
    }
    
    // Timeout check
    if (millis() - startTime > DWM_RESPONSE_TIMEOUT_MS) {
      if (index > 0) {
        buffer[index] = '\0';
        // Return partial if it looks valid
        if (strncmp(buffer, "OK", 2) == 0 || strncmp(buffer, "ERR", 3) == 0) {
          return true;
        }
      }
      return false;
    }
  }
  
  // Buffer full
  buffer[maxLen - 1] = '\0';
  return false;
}

// ============================================================================
// HANDSHAKE FUNCTION
// ============================================================================

/**
 * Perform startup handshake with DWM3001CDK
 */
bool perform_handshake() {
  delay(500);
  
  // Clear any pending data
  while (DWMSerial.available()) DWMSerial.read();
  
  // Ensure DWM listener is active
  if (!DWMSerial.isListening()) {
    DWMSerial.listen();
    delay(10);
  }
  
  // Try handshake 3 times
  for (int attempt = 0; attempt < 3; attempt++) {
    Serial.print(F("[HANDSHAKE] Attempt "));
    Serial.print(attempt + 1);
    Serial.println(F("/3"));
    
    // Clear buffer
    while (DWMSerial.available()) DWMSerial.read();
    delay(100);
    
    // Mark waiting for response
    waitingForDWMResponse = true;
    
    // Send NODE_TYPE command
    dwm_send_command("NODE_TYPE");
    delay(150);
    
    // Wait for response
    bool responseReceived = dwm_receive_response(responseBuffer, COMMAND_BUFFER_SIZE);
    
    // Clear flag
    waitingForDWMResponse = false;
    
    if (responseReceived) {
      Serial.print(F("[HANDSHAKE] Response: "));
      Serial.println(responseBuffer);
      
      // Parse node type
      if (strncmp(responseBuffer, "OK ", 3) == 0) {
        const char* nodeTypeStart = strstr(responseBuffer, "=");
        if (nodeTypeStart != NULL) {
          strncpy(nodeType, nodeTypeStart + 1, sizeof(nodeType) - 1);
          nodeType[sizeof(nodeType) - 1] = '\0';
        } else {
          strncpy(nodeType, responseBuffer + 3, sizeof(nodeType) - 1);
          nodeType[sizeof(nodeType) - 1] = '\0';
        }
        return true;
      }
    }
    
    // Wait before retry
    if (attempt < 2) {
      delay(500);
    }
  }
  
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
  char errorMsg[32];
  snprintf(errorMsg, sizeof(errorMsg), "ERR_%s", errorType);
  Serial.print(F("[ERROR] Sending: "));
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
  
  if (ledRxActive && currentTime >= ledRxOffTime) {
    digitalWrite(LED_RX_PIN, LOW);
    ledRxActive = false;
  }
  
  if (ledTxActive && currentTime >= ledTxOffTime) {
    digitalWrite(LED_TX_PIN, LOW);
    ledTxActive = false;
  }
}
