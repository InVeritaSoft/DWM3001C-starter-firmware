#!/usr/bin/env node
/**
 * Simple PING test
 * Tests if firmware responds to PING command
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2];

if (!portName) {
  console.error("Usage: node scripts/test-ping-simple.js <PORT>");
  process.exit(1);
}

console.log(`Testing PING on ${portName}...\n`);

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

let responses = [];

parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  responses.push(trimmed);
  console.log(`[RX] ${trimmed}`);
});

serialPort.on("open", () => {
  console.log("✓ Port opened\n");
  console.log("Waiting 2 seconds for startup messages...\n");
  
  setTimeout(() => {
    responses = [];
    console.log("Sending PING command...");
    console.log("Watch Orange LED (D10) - should blink if command received!\n");
    
    const pingCmd = "PNG\r\n";
    serialPort.write(pingCmd, (err) => {
      if (!err) {
        console.log(`[TX] ${pingCmd.trim()}`);
      }
    });
    
    setTimeout(() => {
      console.log("\n" + "=".repeat(60));
      console.log("RESULTS");
      console.log("=".repeat(60));
      
      const filtered = responses.filter(r => 
        !r.includes("STARTUP") && 
        !r.includes("DW3000") && 
        !r.includes("MAIN_LOOP")
      );
      
      if (filtered.length === 0) {
        console.log("❌ NO RESPONSES RECEIVED!");
        console.log("\nThis means:");
        console.log("1. Commands not reaching firmware (RS-485 DE/RE not connected?)");
        console.log("2. Or firmware not responding");
        console.log("\nCheck:");
        console.log("- Did Orange LED (D10) blink? (command received)");
        console.log("- Did Green LED (D11) blink? (response sent)");
        console.log("- Is RS-485 DE/RE pin connected? (GPIO P0.06)");
      } else {
        console.log(`✓ Received ${filtered.length} response(s):`);
        filtered.forEach(r => console.log(`   ${r}`));
        
        if (filtered.some(r => r.includes("OK"))) {
          console.log("\n✓ PING successful!");
        }
      }
      
      console.log("\n" + "=".repeat(60));
      serialPort.close();
      process.exit(0);
    }, 2000);
  }, 2000);
});

serialPort.open((err) => {
  if (err) {
    console.error(`[ERROR] Failed to open port: ${err.message}`);
    process.exit(1);
  }
});

