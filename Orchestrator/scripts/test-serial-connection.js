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

// Handle incoming data
parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  responseCount++;
  console.log(`[RX] ${trimmed}`);
  console.log(`[RX] Hex: ${Buffer.from(data).toString("hex")}`);
  
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
  // Only log raw data if it's not already captured by parser
  // This helps identify if data is coming through but parser isn't working
  if (data.length > 0) {
    console.log(`[RAW] ${data.length} bytes: Hex=${hex}, ASCII="${ascii}"`);
  }
});

serialPort.on("error", (error) => {
  console.error(`[ERROR] ${error.message}`);
});

serialPort.on("open", () => {
  console.log(`✓ Port ${portName} opened at 115200 baud\n`);
  console.log("Listening for firmware startup messages...");
  console.log("(Firmware should send: OK STARTUP V2, OK DW3000_READY, OK MAIN_LOOP)\n");
  
  // Wait longer to catch startup messages
  setTimeout(() => {
    console.log("=== Starting connectivity test ===\n");
    
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
        
        if (responseCount === 0) {
          console.log("\n❌ No responses received!");
          console.log("\nThis indicates one of the following:");
          console.log("\n1. Firmware not running orchestrator v2:");
          console.log("   - Check which firmware is currently flashed");
          console.log("   - Rebuild and flash:");
          console.log("     Node A: .\\build-and-flash-tx.ps1");
          console.log("     Node B: .\\build-and-flash-rx.ps1");
          console.log("   - Verify firmware starts by checking LEDs on power-on");
          console.log("\n2. Hardware connection issue:");
          console.log("   - RS-485 transceiver may not be connected");
          console.log("   - Check wiring: A+/B- lines, GND");
          console.log("   - Verify termination resistors (120Ω at each end)");
          console.log("   - Check transceiver enable/DE pins");
          console.log("\n3. UART not working:");
          console.log("   - Firmware may have crashed during UART init");
          console.log("   - Check if firmware sends startup messages on power-on");
          console.log("   - Watch for LED patterns (red LED blinking = error)");
          console.log("\n4. Port/baud rate mismatch:");
          console.log("   - Verify port name is correct");
          console.log("   - Ensure baud rate is 115200");
          console.log("   - Check no other software is using the port");
          console.log("\nNext steps:");
          console.log("1. Power cycle the board and watch for startup messages");
          console.log("2. Check LED behavior (should blink on startup)");
          console.log("3. Verify firmware file was flashed correctly");
          console.log("4. Test with direct serial terminal (PuTTY/Tera Term)");
        } else if (responseCount < commandCount) {
          console.log(`\n⚠ Only ${responseCount} of ${commandCount} commands received responses`);
          console.log("Some communication is working, but not all commands are being answered.");
        } else {
          console.log("\n✓ All commands received responses!");
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

