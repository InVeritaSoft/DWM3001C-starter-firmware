#!/usr/bin/env node
/**
 * Simple serial port connectivity test
 * Tests if firmware is responding to commands
 * 
 * Usage: node scripts/test-serial-connection.js <COM_PORT>
 * Example: node scripts/test-serial-connection.js COM17
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2];

if (!portName) {
  console.error("Usage: node scripts/test-serial-connection.js <COM_PORT>");
  console.error("Example: node scripts/test-serial-connection.js COM17");
  process.exit(1);
}

console.log(`Testing serial connection on ${portName}...`);
console.log("Press Ctrl+C to exit\n");

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

let commandCount = 0;
let responseCount = 0;
let startupMessages = [];
let rawDataReceived = [];
const expectedStartupMessages = [
  "OK STARTUP V2",
  "OK DW3000_READY",
  "OK MAIN_LOOP"
];

// Handle incoming data
parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  responseCount++;
  console.log(`[RX] ${trimmed}`);
  console.log(`[RX] Hex: ${Buffer.from(data).toString("hex")}`);
  
  // Track startup messages
  if (trimmed.includes("STARTUP") || trimmed.includes("DW3000") || trimmed.includes("MAIN_LOOP")) {
    startupMessages.push(trimmed);
  }
  
  if (trimmed.startsWith("OK")) {
    console.log("✓ Response received successfully!\n");
  } else if (trimmed.startsWith("ERR")) {
    console.log("⚠ Firmware returned error\n");
  }
});

// Handle raw data (in case parser misses something)
serialPort.on("data", (data) => {
  const hex = data.toString("hex");
  const ascii = data.toString("ascii").replace(/[^\x20-\x7E\r\n]/g, ".");
  // Track raw data to detect if data is coming but parser isn't working
  if (data.length > 0) {
    rawDataReceived.push({ hex, ascii, length: data.length });
    // Only log if it looks like it might not be parsed correctly
    // (contains non-printable chars or doesn't match expected format)
    const hasNonPrintable = /[^\x20-\x7E\r\n]/.test(data.toString("ascii"));
    if (hasNonPrintable || data.length > 50) {
      console.log(`[RAW] ${data.length} bytes: Hex=${hex}, ASCII="${ascii}"`);
      if (hasNonPrintable) {
        console.log(`[RAW] ⚠️  Contains non-printable characters - possible baud rate mismatch!`);
      }
    }
  }
});

serialPort.on("error", (error) => {
  console.error(`[ERROR] ${error.message}`);
});

serialPort.on("open", () => {
  console.log(`✓ Port ${portName} opened at 115200 baud\n`);
  console.log("Listening for firmware startup messages...");
  console.log("(Firmware should send: OK STARTUP V2, OK DW3000_READY, OK MAIN_LOOP)");
  console.log("NOTE: If board was just powered on, startup messages may appear now.\n");
  console.log("If no startup messages appear, the firmware may not be running or UART is not working.\n");
  
  // Wait longer to catch startup messages (5 seconds to allow for board power-on)
  setTimeout(() => {
    console.log("\n=== Startup Messages Summary ===");
    if (startupMessages.length > 0) {
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
      }
    } else {
      console.log("❌ No startup messages received!");
      
      // Check if raw data was received but not parsed
      if (rawDataReceived.length > 0) {
        console.log(`\n⚠️  WARNING: Raw data was received (${rawDataReceived.length} chunks) but not parsed!`);
        console.log("This suggests:");
        console.log("  1. Baud rate mismatch (firmware sending at different rate)");
        console.log("  2. Data corruption");
        console.log("  3. Parser delimiter issue");
        console.log("\nRaw data samples:");
        rawDataReceived.slice(0, 3).forEach((raw, idx) => {
          console.log(`   ${idx + 1}. ${raw.length} bytes: ${raw.hex.substring(0, 40)}...`);
        });
        console.log("\nTry:");
        console.log("  - Verify baud rate is exactly 115200");
        console.log("  - Check for electrical interference");
        console.log("  - Test with direct serial terminal to see raw output");
      } else {
        console.log("\nThis indicates:");
        console.log("  1. Firmware is not running orchestrator v2");
        console.log("  2. UART is not working (check wiring, baud rate)");
        console.log("  3. Board needs to be power cycled (unplug and reconnect)");
        console.log("  4. Firmware crashed before sending startup messages");
      }
    }
    
    console.log("\n=== Starting connectivity test ===\n");
    
    // Test 1: PING
    console.log("[TEST 1] Sending PING command...");
    commandCount++;
    const pingCmd = "PNG\r\n";
    serialPort.write(pingCmd, (err) => {
      if (err) {
        console.error(`[ERROR] Write failed: ${err.message}`);
      } else {
        console.log(`[TX] ${pingCmd.trim()} (${pingCmd.length} bytes)`);
        console.log(`[TX] Hex: ${Buffer.from(pingCmd).toString("hex")}`);
      }
    });
    
    // Test 2: NODE_TYPE (after 3 seconds)
    setTimeout(() => {
      console.log("\n[TEST 2] Sending NODE_TYPE command...");
      commandCount++;
      const nodeTypeCmd = "NODE_TYPE\r\n";
      serialPort.write(nodeTypeCmd, (err) => {
        if (err) {
          console.error(`[ERROR] Write failed: ${err.message}`);
        } else {
          console.log(`[TX] ${nodeTypeCmd.trim()} (${nodeTypeCmd.length} bytes)`);
          console.log(`[TX] Hex: ${Buffer.from(nodeTypeCmd).toString("hex")}`);
        }
      });
      
      // Summary after 5 seconds
      setTimeout(() => {
        console.log("\n=== Test Summary ===");
        console.log(`Commands sent: ${commandCount}`);
        console.log(`Responses received: ${responseCount}`);
        console.log(`Startup messages received: ${startupMessages.length}`);
        
        if (responseCount === 0) {
          console.log("\n❌ No responses received!");
          
          if (startupMessages.length === 0) {
            console.log("\n⚠️  CRITICAL: No startup messages AND no command responses!");
            console.log("\nThis strongly suggests:");
            console.log("  1. Firmware is NOT running (or wrong firmware flashed)");
            console.log("  2. UART communication is completely broken");
            console.log("  3. Board needs to be power cycled");
            console.log("\nImmediate actions:");
            console.log("  1. Power cycle the board (unplug USB, wait 2 seconds, reconnect)");
            console.log("  2. Watch for startup messages immediately after power-on");
            console.log("  3. Check LED behavior:");
            console.log("     - Red LED should blink twice on startup");
            console.log("     - Blue LED should turn on (indicates UART initialized)");
            console.log("     - If red LED blinks rapidly, firmware crashed");
            console.log("  4. Verify correct firmware is flashed:");
            console.log("     - Node A (TX): Should be orchestrator_tx_v2");
            console.log("     - Node B (RX): Should be orchestrator_rx_v2");
            console.log("  5. Test with direct serial terminal:");
            console.log("     - Linux: screen /dev/ttyUSB0 115200");
            console.log("     - Windows: PuTTY COM17/COM18, 115200, 8N1");
            console.log("     - Power cycle board and watch for startup messages");
          } else {
            console.log("\n⚠️  Startup messages received but commands not responding!");
            console.log("\nThis suggests:");
            console.log("  1. Firmware is running but command parsing may be broken");
            console.log("  2. RS-485 transceiver may not be switching to RX mode");
            console.log("  3. Commands are not reaching the firmware");
            console.log("\nCheck:");
            console.log("  - RS-485 transceiver DE/RE pins (direction control)");
            console.log("  - Watch Orange LED (D10) - should blink when command received");
            console.log("  - Watch Green LED (D11) - should blink when response sent");
            console.log("  - Verify RS-485 wiring (A+/B-, GND, termination resistors)");
          }
          
          console.log("\n2. Hardware connection issue:");
          console.log("   - RS-485 transceiver may not be connected");
          console.log("   - Check wiring: A+/B- lines, GND");
          console.log("   - Verify termination resistors (120Ω at each end)");
          console.log("   - Check transceiver enable/DE pins");
          console.log("\n3. Port/baud rate mismatch:");
          console.log("   - Verify port name is correct");
          console.log("   - Ensure baud rate is 115200");
          console.log("   - Check no other software is using the port");
        } else if (responseCount < commandCount) {
          console.log(`\n⚠ Only ${responseCount} of ${commandCount} commands received responses`);
          console.log("Some communication is working, but not all commands are being answered.");
        } else {
          console.log("\n✓ All commands received responses!");
          if (startupMessages.length > 0) {
            console.log("✓ Startup messages also received - firmware is running correctly!");
          }
        }
        
        console.log("\nTest complete. Press Ctrl+C to exit.");
      }, 5000);
    }, 3000);
  }, 2000);
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

// Handle Ctrl+C gracefully
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

