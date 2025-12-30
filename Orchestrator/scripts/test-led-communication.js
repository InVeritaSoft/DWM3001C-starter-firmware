#!/usr/bin/env node
/**
 * Test LED Communication - Watch LEDs while sending commands
 * This helps diagnose if commands are reaching the firmware
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2] || "/dev/ttyUSB0";

console.log("=".repeat(60));
console.log("LED COMMUNICATION TEST");
console.log("=".repeat(60));
console.log(`Port: ${portName}`);
console.log("");
console.log("LED GUIDE:");
console.log("  D9 (Red)   = Startup/Error");
console.log("  D10 (Orange) = Command received (WATCH THIS!)");
console.log("  D11 (Green)  = Response sent (WATCH THIS!)");
console.log("  D13 (Yellow) = DW3000 status");
console.log("");
console.log("=".repeat(60));
console.log("TEST: Watch Node A LEDs while sending commands");
console.log("=".repeat(60));
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

let responseCount = 0;

parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  if (trimmed.length > 0) {
    responseCount++;
    console.log(`[RX] ${trimmed}`);
  }
});

serialPort.on("open", () => {
  console.log("✓ Port opened\n");
  
  const sendCommand = (cmd, description) => {
    return new Promise((resolve) => {
      responseCount = 0;
      console.log(`\n${"=".repeat(60)}`);
      console.log(`Sending: ${cmd}`);
      console.log(`Purpose: ${description}`);
      console.log(`${"=".repeat(60)}`);
      console.log("→ WATCH D10 (Orange) LED - should blink if command received!");
      console.log("→ WATCH D11 (Green) LED - should blink if response sent!");
      console.log("");
      
      const timeout = setTimeout(() => {
        console.log(`\n[RESULT] Response received: ${responseCount > 0 ? "YES" : "NO"}`);
        if (responseCount === 0) {
          console.log("  ❌ NO RESPONSE - Check:");
          console.log("     - Did D10 (Orange) blink? → If NO: Command not reaching firmware");
          console.log("     - Did D11 (Green) blink? → If NO: Response not being sent");
          console.log("     - If both blinked but no response: RS-485 RX issue on RPi5");
        } else {
          console.log("  ✓ Response received!");
        }
        resolve();
      }, 2000);
      
      const fullCmd = cmd + "\r\n";
      serialPort.write(fullCmd, (err) => {
        if (!err) {
          console.log(`[TX] ${cmd}`);
        } else {
          clearTimeout(timeout);
          console.error(`[ERROR] Write failed: ${err.message}`);
          resolve();
        }
      });
    });
  };
  
  (async () => {
    // Wait for startup
    console.log("Waiting 2 seconds for firmware startup...\n");
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    // Test 1: PING
    await sendCommand("PNG", "Test basic communication");
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // Test 2: NODE_TYPE
    await sendCommand("NODE_TYPE", "Get node type");
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // Test 3: STAT
    await sendCommand("STAT", "Get statistics");
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    console.log("\n" + "=".repeat(60));
    console.log("TEST COMPLETE");
    console.log("=".repeat(60));
    console.log("\nSummary:");
    console.log("  - If D10 (Orange) blinked: Commands ARE reaching firmware ✓");
    console.log("  - If D10 (Orange) did NOT blink: Commands NOT reaching firmware ✗");
    console.log("  - If D11 (Green) blinked: Responses ARE being sent ✓");
    console.log("  - If D11 (Green) did NOT blink: Responses NOT being sent ✗");
    console.log("  - If both blinked but no response: RS-485 RX issue on RPi5");
    console.log("");
    
    serialPort.close();
    process.exit(0);
  })();
});

serialPort.open((err) => {
  if (err) {
    console.error(`[ERROR] Failed to open port: ${err.message}`);
    process.exit(1);
  }
});

