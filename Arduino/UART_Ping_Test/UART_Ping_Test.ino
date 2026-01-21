/*
 * UART Ping Test Firmware for Arduino Uno
 * 
 * Purpose: Simple UART communication test with DWM3001CDK
 *          - Receives "Hello World" from DWM3001CDK
 *          - Sends back "PING" when "Hello World" is received
 * 
 * Pin Assignments:
 *   DWM3001CDK Communication:
 *     - D8  = RX (from DWM3001CDK TX - GPIO 27, J10 Pin 19, GPIO27_PIN19_TX)
 *     - D9  = TX (to DWM3001CDK RX - GPIO 15, J10 Pin 10)
 *   
 *   LED Indicators:
 *     - D13 = Built-in LED (blinks when "Hello World" received)
 * 
 * Communication Protocol:
 *   - Baud Rate: 115200
 *   - DWM3001CDK sends: "Hello World\r\n"
 *   - Arduino responds: "PING"
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

// LED Indicator Pins
#define LED_BUILTIN_PIN 13    // Built-in LED

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

#define DWM_BAUD_RATE 115200          // DWM3001CDK baud rate
#define MESSAGE_BUFFER_SIZE 64       // Buffer size for received messages
#define HELLO_WORLD_MSG "Hello World" // Message to detect
#define PING_RESPONSE "PING"          // Response message
#define LED_BLINK_DURATION_MS 100     // LED blink duration

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Serial port instance for DWM3001CDK
SoftwareSerial DWMSerial(DWM_RX_PIN, DWM_TX_PIN);

// Message buffer
char messageBuffer[MESSAGE_BUFFER_SIZE];
uint8_t bufferIndex = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize built-in LED
    pinMode(LED_BUILTIN_PIN, OUTPUT);
    digitalWrite(LED_BUILTIN_PIN, LOW);
    
    // Initialize serial for debugging (USB)
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port to connect
    }
    
    // Initialize SoftwareSerial for DWM3001CDK
    DWMSerial.begin(DWM_BAUD_RATE);
    
    // Startup indication: blink LED 3 times
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN_PIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN_PIN, LOW);
        delay(200);
    }
    
    Serial.println("Arduino UART Ping Test - Ready");
    Serial.println("Waiting for 'Hello World' from DWM3001CDK...");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Check if data is available from DWM3001CDK
    while (DWMSerial.available() > 0) {
        char c = DWMSerial.read();
        
        // Add character to buffer if there's space
        if (bufferIndex < (MESSAGE_BUFFER_SIZE - 1)) {
            messageBuffer[bufferIndex++] = c;
            messageBuffer[bufferIndex] = '\0'; // Null terminate
            
            // Check if we received "Hello World"
            if (strstr(messageBuffer, HELLO_WORLD_MSG) != NULL) {
                // "Hello World" detected!
                Serial.println("Received: Hello World");
                
                // Blink LED to indicate reception
                digitalWrite(LED_BUILTIN_PIN, HIGH);
                delay(LED_BLINK_DURATION_MS);
                digitalWrite(LED_BUILTIN_PIN, LOW);
                
                // Send "PING" response
                DWMSerial.print(PING_RESPONSE);
                Serial.println("Sent: PING");
                
                // Reset buffer
                bufferIndex = 0;
                messageBuffer[0] = '\0';
            }
            
            // If buffer is getting full, check for newline and reset
            if (c == '\n' || c == '\r') {
                // Newline received, reset buffer for next message
                bufferIndex = 0;
                messageBuffer[0] = '\0';
            }
        } else {
            // Buffer full, reset it
            bufferIndex = 0;
            messageBuffer[0] = '\0';
        }
    }
    
    // Small delay to prevent busy waiting
    delay(10);
}
