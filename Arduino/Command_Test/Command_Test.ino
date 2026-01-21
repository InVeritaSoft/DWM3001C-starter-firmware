/*
 * Command Test Firmware for Arduino Uno
 * 
 * Purpose: Bridge between Serial Monitor and DWM3001CDK orchestrator_v2
 *          - Receives commands from Serial Monitor (USB)
 *          - Forwards commands to DWM3001CDK via UART
 *          - Receives responses from DWM3001CDK
 *          - Displays responses in Serial Monitor
 * 
 * Pin Assignments:
 *   DWM3001CDK Communication:
 *     - D8  = RX (from DWM3001CDK TX - GPIO 27, J10 Pin 19, GPIO27_PIN19_TX)
 *     - D9  = TX (to DWM3001CDK RX - GPIO 15, J10 Pin 10)
 *   
 *   Node Identification:
 *     - Connect A0 to GND for Node A (TX)
 *     - Leave A0 floating or connect to 5V for Node B (RX)
 * 
 * Supported Commands:
 *   - PING (or PNG)
 *   - NODE_TYPE
 *   - START (or STRT or START_TEST)
 *   - STOP (or STOP_TEST)
 *   - STATS (or STAT or GET_STATS)
 *   - CONFIG (or CFG or SET_CONFIG) - e.g., "CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100"
 *   - RESET_STATS (or RST)
 * 
 * Communication Protocol:
 *   - Baud Rate: 115200
 *   - Commands sent with \r\n delimiter
 *   - Responses received with \r\n delimiter
 * 
 * Author: Generated for INVERITA DWM3001C Test Rig
 * Version: 1.0
 * Date: 2026-01-20
 */

#include <SoftwareSerial.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// DWM3001CDK Communication Pins
#define DWM_RX_PIN 8          // Arduino RX from DWM TX (GPIO27, J10 Pin 19, GPIO27_PIN19_TX)
#define DWM_TX_PIN 9          // Arduino TX to DWM RX (GPIO15, J10 Pin 10)

// Node Identification Pin
#define NODE_ID_PIN A0        // Connect to GND for Node A, floating/5V for Node B

// LED Indicator Pins
#define LED_BUILTIN_PIN 13    // Built-in LED

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

#define DWM_BAUD_RATE 115200
#define SERIAL_BAUD_RATE 115200
#define COMMAND_TIMEOUT_MS 3000
#define RESPONSE_BUFFER_SIZE 512
#define COMMAND_BUFFER_SIZE 256

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

SoftwareSerial DWMSerial(DWM_RX_PIN, DWM_TX_PIN);

char responseBuffer[RESPONSE_BUFFER_SIZE];
uint16_t responseIndex = 0;
unsigned long lastCommandTime = 0;
bool waitingForResponse = false;

// Node identification
bool isNodeA = false;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize serial communication
    Serial.begin(SERIAL_BAUD_RATE);
    DWMSerial.begin(DWM_BAUD_RATE);
    
    // Configure node identification pin
    pinMode(NODE_ID_PIN, INPUT_PULLUP);
    
    // Configure LED
    pinMode(LED_BUILTIN_PIN, OUTPUT);
    
    // Determine node type
    // A0 connected to GND = Node A (TX)
    // A0 floating/5V = Node B (RX)
    delay(100); // Wait for pin to stabilize
    isNodeA = (digitalRead(NODE_ID_PIN) == LOW);
    
    // Startup indication
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN_PIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN_PIN, LOW);
        delay(200);
    }
    
    // Print startup message
    Serial.println();
    Serial.println("========================================");
    Serial.println("DWM3001CDK Command Test Bridge");
    Serial.print("Node Type: ");
    Serial.println(isNodeA ? "A (TX)" : "B (RX)");
    Serial.println("========================================");
    Serial.println("Ready! Type commands and press Enter:");
    Serial.println("  - PING");
    Serial.println("  - NODE_TYPE");
    Serial.println("  - START");
    Serial.println("  - STOP");
    Serial.println("  - STATS");
    Serial.println("  - CONFIG <params>");
    Serial.println("  - RESET_STATS");
    Serial.println("========================================");
    Serial.println();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Check for commands from Serial Monitor
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim(); // Remove whitespace
        
        if (command.length() > 0) {
            sendCommandToDWM(command);
        }
    }
    
    // Check for responses from DWM3001CDK
    // Process all available characters
    while (DWMSerial.available() > 0) {
        char c = DWMSerial.read();
        processResponseChar(c);
    }
    
    // Check for timeout
    if (waitingForResponse && (millis() - lastCommandTime > COMMAND_TIMEOUT_MS)) {
        Serial.print("[TIMEOUT] No response from DWM3001CDK (received ");
        Serial.print(responseIndex);
        Serial.println(" chars so far)");
        
        // If we have partial data, show it
        if (responseIndex > 0) {
            responseBuffer[responseIndex] = '\0';
            Serial.print("[PARTIAL] ");
            Serial.println(responseBuffer);
        }
        
        waitingForResponse = false;
        responseIndex = 0;
    }
}

// ============================================================================
// COMMAND HANDLING
// ============================================================================

void sendCommandToDWM(String command) {
    // Clear response buffer
    responseIndex = 0;
    memset(responseBuffer, 0, sizeof(responseBuffer));
    
    // Print command being sent
    Serial.print("[SEND] ");
    Serial.println(command);
    
    // Small delay to ensure UART is ready
    delay(10);
    
    // Send command to DWM3001CDK with \r\n delimiter
    DWMSerial.print(command);
    DWMSerial.print("\r\n");
    
    // Flush to ensure data is sent
    DWMSerial.flush();
    
    // Update state
    lastCommandTime = millis();
    waitingForResponse = true;
    
    // Blink LED to indicate command sent
    digitalWrite(LED_BUILTIN_PIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN_PIN, LOW);
}

// ============================================================================
// RESPONSE HANDLING
// ============================================================================

void processResponseChar(char c) {
    // Always buffer incoming data, even if not explicitly waiting
    // This handles cases where responses come before we set waitingForResponse
    
    // Handle line endings
    if (c == '\n' || c == '\r') {
        // Skip empty lines (just \r or \n)
        if (responseIndex == 0) {
            return;
        }
        
        // Null-terminate the response
        responseBuffer[responseIndex] = '\0';
        
        // Print response
        if (waitingForResponse) {
            Serial.print("[RECV] ");
        } else {
            Serial.print("[UNSOLICITED] ");
        }
        Serial.println(responseBuffer);
        
        // Reset state
        waitingForResponse = false;
        responseIndex = 0;
        
        // Blink LED to indicate response received
        digitalWrite(LED_BUILTIN_PIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN_PIN, LOW);
    } else if (responseIndex < (RESPONSE_BUFFER_SIZE - 1)) {
        // Add character to buffer
        responseBuffer[responseIndex++] = c;
    } else {
        // Buffer overflow - reset
        Serial.println("[ERROR] Response buffer overflow!");
        responseIndex = 0;
        waitingForResponse = false;
    }
}
