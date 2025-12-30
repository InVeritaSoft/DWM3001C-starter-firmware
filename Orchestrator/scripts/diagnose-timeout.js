#!/usr/bin/env node
/**
 * Comprehensive timeout diagnosis script
 * Tests firmware communication and provides detailed diagnostics
 * 
 * Usage: node scripts/diagnose-timeout.js <COM_PORT>
 * Example: node scripts/diagnose-timeout.js /dev/ttyUSB0
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2];

if (!portName) {
  console.error("Usage: node scripts/diagnose-timeout.js <COM_PORT>");
  console.error("Example: node scripts/diagnose-timeout.js /dev/ttyUSB0");
  process.exit(1);
}

console.log("=".repeat(60));
console.log("RS-485 TIMEOUT DIAGNOSTIC TOOL");
console.log("=".repeat(60));
console.log(`Testing port: ${portName}`);
console.log(`Baud rate: 115200`);
console.log("");

const serialPort = new SerialPort({
  path: portName,
  baudRate: 115200,
  dataBits: 8,
  parity: "none",
  stopBits: 1,
  rtscts: false,
  autoOpen: false,
});

const parser = serialPort.pipe(new ReadlineParser({ delimiter: "\r\n" }));

let startupMessages = [];
let commandResponses = [];
let rawDataChunks = [];
let testResults = {
  startupMessagesReceived: false,
  commandsResponded: false,
  blueLEDExpected: false,
  orangeLEDExpected: false,
  greenLEDExpected: false,
};

const expectedStartupMessages = [
  "OK STARTUP V2",
  "OK DW3000_READY",
  "OK MAIN_LOOP"
];

// Track all incoming data
parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  console.log(`[RX] ${trimmed}`);
  
  if (trimmed.includes("STARTUP") || trimmed.includes("DW3000") || trimmed.includes("MAIN_LOOP")) {
    startupMessages.push(trimmed);
    testResults.startupMessagesReceived = true;
  } else if (trimmed.startsWith("OK") || trimmed.startsWith("ERR")) {
    commandResponses.push(trimmed);
    testResults.commandsResponded = true;
  }
});

// Track raw data
serialPort.on("data", (data) => {
  rawDataChunks.push({
    hex: data.toString("hex"),
    length: data.length,
    timestamp: Date.now()
  });
});

serialPort.on("error", (error) => {
  console.error(`[ERROR] ${error.message}`);
});

serialPort.on("open", () => {
  console.log("✓ Port opened successfully\n");
  console.log("=".repeat(60));
  console.log("STEP 1: CHECKING STARTUP MESSAGES");
  console.log("=".repeat(60));
  console.log("Waiting 5 seconds for firmware startup messages...");
  console.log("Expected messages:");
  expectedStartupMessages.forEach((msg, idx) => {
    console.log(`  ${idx + 1}. ${msg}`);
  });
  console.log("");
  console.log("LED CHECK:");
  console.log("  - Blue LED (LED 3) should be ON (indicates UART initialized)");
  console.log("  - Red LED (LED 0) should blink twice then stay OFF");
  console.log("");
  
  setTimeout(() => {
    console.log("\n" + "=".repeat(60));
    console.log("STARTUP MESSAGE ANALYSIS");
    console.log("=".repeat(60));
    
    if (startupMessages.length === 0) {
      console.log("❌ NO STARTUP MESSAGES RECEIVED!\n");
      console.log("This indicates:");
      console.log("  1. Firmware is NOT running (or wrong firmware flashed)");
      console.log("  2. UART communication is completely broken");
      console.log("  3. Board needs to be power cycled\n");
      console.log("IMMEDIATE ACTIONS:");
      console.log("  1. Power cycle the board (unplug USB, wait 2 seconds, reconnect)");
      console.log("  2. Watch for startup messages immediately after power-on");
      console.log("  3. Check LED behavior:");
      console.log("     - Red LED should blink twice on startup");
      console.log("     - Blue LED should turn ON (indicates UART initialized)");
      console.log("     - If red LED blinks rapidly (20 times), firmware crashed");
      console.log("  4. Verify correct firmware is flashed:");
      console.log("     - Node A (TX): orchestrator_tx_v2");
      console.log("     - Node B (RX): orchestrator_rx_v2");
      console.log("  5. Test with direct serial terminal:");
      console.log(`     - Linux: screen ${portName} 115200`);
      console.log(`     - Windows: PuTTY ${portName}, 115200, 8N1`);
      console.log("     - Power cycle board and watch for startup messages");
      
      if (rawDataChunks.length > 0) {
        console.log("\n⚠️  WARNING: Raw data was received but not parsed!");
        console.log(`   Received ${rawDataChunks.length} data chunks`);
        console.log("   This suggests baud rate mismatch or data corruption");
        console.log("   Sample hex:", rawDataChunks[0].hex.substring(0, 40));
      }
    } else {
      console.log(`✓ Received ${startupMessages.length} startup message(s):`);
      startupMessages.forEach((msg, idx) => {
        console.log(`   ${idx + 1}. ${msg}`);
      });
      
      const missing = expectedStartupMessages.filter(
        expected => !startupMessages.some(msg => msg.includes(expected.split(" ")[1]))
      );
      
      if (missing.length > 0) {
        console.log(`\n⚠ Missing startup messages:`);
        missing.forEach(msg => console.log(`   - ${msg}`));
        console.log("\nThis may indicate:");
        console.log("  - Firmware crashed during initialization");
        console.log("  - DW3000 initialization failed");
        console.log("  - Firmware is not orchestrator v2");
      } else {
        console.log("\n✓ All startup messages received - firmware is running!");
      }
    }
    
    console.log("\n" + "=".repeat(60));
    console.log("STEP 2: TESTING COMMAND RESPONSES");
    console.log("=".repeat(60));
    console.log("Sending PING command...");
    console.log("Watch LEDs:");
    console.log("  - Orange LED (LED 1) should blink when command received");
    console.log("  - Green LED (LED 2) should blink when response sent");
    console.log("");
    
    const pingCmd = "PNG\r\n";
    serialPort.write(pingCmd, (err) => {
      if (err) {
        console.error(`[ERROR] Write failed: ${err.message}`);
      } else {
        console.log(`[TX] ${pingCmd.trim()}`);
        console.log(`[TX] Hex: ${Buffer.from(pingCmd).toString("hex")}`);
      }
    });
    
    setTimeout(() => {
      console.log("\n" + "=".repeat(60));
      console.log("COMMAND RESPONSE ANALYSIS");
      console.log("=".repeat(60));
      
      if (commandResponses.length === 0) {
        console.log("❌ NO COMMAND RESPONSES RECEIVED!\n");
        
        if (startupMessages.length > 0) {
          console.log("⚠️  CRITICAL: Startup messages received but commands not responding!");
          console.log("\nThis suggests:");
          console.log("  1. Firmware is running but command parsing may be broken");
          console.log("  2. RS-485 transceiver may not be switching to RX mode");
          console.log("  3. Commands are not reaching the firmware");
          console.log("\nCHECK:");
          console.log("  - RS-485 transceiver DE/RE pins (direction control)");
          console.log("     * DE/RE should be connected to GPIO P0.06");
          console.log("     * DE/RE should be LOW (0V) in RX mode");
          console.log("     * DE/RE should be HIGH (3.3V) when firmware sends response");
          console.log("  - Watch Orange LED (LED 1) - should blink when command received");
          console.log("  - Watch Green LED (LED 2) - should blink when response sent");
          console.log("  - Verify RS-485 wiring:");
          console.log("     * RO (Receive Out) → GPIO 15 (Pin 10 on J10)");
          console.log("     * DI (Data In) ← GPIO 14 (Pin 8 on J10)");
          console.log("     * GND → Pin 6 on J10");
          console.log("     * A+/B- lines connected correctly");
          console.log("     * Termination resistors (120Ω at each end)");
          console.log("  - RS-485 transceiver power:");
          console.log("     * VCC should be 3.3V or 5V (depending on transceiver)");
          console.log("     * GND must be connected");
        } else {
          console.log("⚠️  No startup messages AND no command responses!");
          console.log("This indicates firmware is not running or UART is broken.");
        }
      } else {
        console.log(`✓ Received ${commandResponses.length} response(s):`);
        commandResponses.forEach((resp, idx) => {
          console.log(`   ${idx + 1}. ${resp}`);
        });
        console.log("\n✓ Commands are working!");
      }
      
      console.log("\n" + "=".repeat(60));
      console.log("FINAL DIAGNOSIS");
      console.log("=".repeat(60));
      console.log("Summary:");
      console.log(`  Startup messages: ${startupMessages.length > 0 ? "✓ YES" : "❌ NO"}`);
      console.log(`  Command responses: ${commandResponses.length > 0 ? "✓ YES" : "❌ NO"}`);
      console.log(`  Raw data chunks: ${rawDataChunks.length}`);
      
      console.log("\n" + "=".repeat(60));
      console.log("HARDWARE CHECKLIST");
      console.log("=".repeat(60));
      console.log("Verify these connections:");
      console.log("");
      console.log("1. RS-485 Transceiver Connections:");
      console.log("   ✓ RO (Receive Out) → GPIO 15 (P0.15) - Pin 10 on J10");
      console.log("   ✓ DI (Data In) ← GPIO 14 (P0.14) - Pin 8 on J10");
      console.log("   ✓ GND → Pin 6 on J10");
      console.log("   ✓ DE/RE → GPIO P0.06 (for direction control)");
      console.log("   ✓ A+ → RS-485 A+ line");
      console.log("   ✓ B- → RS-485 B- line");
      console.log("");
      console.log("2. Power:");
      console.log("   ✓ RS-485 transceiver VCC connected (3.3V or 5V)");
      console.log("   ✓ RS-485 transceiver GND connected");
      console.log("   ✓ Board powered via USB");
      console.log("");
      console.log("3. Termination:");
      console.log("   ✓ 120Ω resistor between A+ and B- at each end of bus");
      console.log("");
      console.log("4. LED Behavior (on board):");
      console.log("   ✓ Blue LED (LED 3) ON = UART initialized");
      console.log("   ✓ Orange LED (LED 1) blinks = Command received");
      console.log("   ✓ Green LED (LED 2) blinks = Response sent");
      console.log("   ✓ Red LED (LED 0) blinks rapidly = Firmware error");
      console.log("");
      console.log("5. Serial Port:");
      console.log(`   ✓ Port ${portName} is correct`);
      console.log("   ✓ Baud rate is 115200");
      console.log("   ✓ No other software using the port");
      console.log("");
      
      console.log("=".repeat(60));
      console.log("Test complete. Press Ctrl+C to exit.");
      console.log("=".repeat(60));
    }, 3000);
  }, 5000);
});

serialPort.open((err) => {
  if (err) {
    console.error(`[ERROR] Failed to open port: ${err.message}`);
    console.error("\nTroubleshooting:");
    console.error("1. Check if port name is correct");
    console.error("2. Ensure no other software is using the port");
    console.error("3. Verify device is connected and powered");
    process.exit(1);
  }
});

process.on("SIGINT", () => {
  console.log("\n\nClosing port...");
  serialPort.close((err) => {
    if (err) {
      console.error(`[ERROR] Failed to close port: ${err.message}`);
    } else {
      console.log("Port closed.");
    }
    process.exit(0);
  });
});

