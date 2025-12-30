#!/usr/bin/env node
/**
 * Check UWB Communication Status
 * Queries Node A to see if it's configured and sending packets
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2] || "/dev/ttyUSB0";

console.log("=".repeat(60));
console.log("CHECK UWB STATUS - NODE A");
console.log("=".repeat(60));
console.log(`Port: ${portName}`);
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

let responses = [];
let startupComplete = false;

parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  if (trimmed.includes("MAIN_LOOP")) {
    startupComplete = true;
  }
  if (trimmed.length > 0 && 
      !trimmed.includes("STARTUP") && 
      !trimmed.includes("DW3000_READY") && 
      !trimmed.includes("MAIN_LOOP")) {
    responses.push(trimmed);
    console.log(`[RX] ${trimmed}`);
  }
});

serialPort.on("open", () => {
  console.log("✓ Port opened\n");
  console.log("Waiting for startup...\n");
  
  setTimeout(() => {
    if (!startupComplete) {
      console.log("⚠ Startup messages not received, but continuing...\n");
    }
    
    responses = [];
    
    const sendCommand = (cmd, waitTime = 2000) => {
      return new Promise((resolve) => {
        responses = [];
        const timeout = setTimeout(() => {
          resolve(responses);
        }, waitTime);
        
        const fullCmd = cmd + "\r\n";
        serialPort.write(fullCmd, (err) => {
          if (!err) {
            console.log(`[TX] ${cmd}`);
          } else {
            clearTimeout(timeout);
            resolve([]);
          }
        });
      });
    };
    
    (async () => {
      try {
        // Check Node Type
        console.log("=".repeat(60));
        console.log("1. Node Type Check");
        console.log("=".repeat(60));
        const nodeType = await sendCommand("NODE_TYPE");
        const isTX = nodeType.some(r => r.includes("TX"));
        console.log(`   Response: ${nodeType.join(", ")}`);
        console.log(`   Status: ${isTX ? "✓ Node A (TX)" : "✗ Not Node A"}\n`);
        
        // Check Configuration
        console.log("=".repeat(60));
        console.log("2. Configuration Status");
        console.log("=".repeat(60));
        const config = await sendCommand("CFG");
        const isConfigured = config.some(r => r.includes("OK CONFIG"));
        console.log(`   Response: ${config.join(", ")}`);
        if (isConfigured) {
          console.log(`   Status: ✓ Configured\n`);
        } else {
          console.log(`   Status: ✗ Not configured\n`);
          console.log("   Action needed: Send CFG command to configure Node A\n");
        }
        
        // Check Statistics
        console.log("=".repeat(60));
        console.log("3. UWB Transmission Statistics");
        console.log("=".repeat(60));
        const stats = await sendCommand("STAT");
        const statsStr = stats.join(" ");
        console.log(`   Response: ${statsStr}`);
        
        const sentMatch = statsStr.match(/sent=(\d+)/);
        const attemptedMatch = statsStr.match(/attempted=(\d+)/);
        const errorsMatch = statsStr.match(/errors=(\d+)/);
        
        const sent = sentMatch ? parseInt(sentMatch[1]) : 0;
        const attempted = attemptedMatch ? parseInt(attemptedMatch[1]) : 0;
        const errors = errorsMatch ? parseInt(errorsMatch[1]) : 0;
        
        console.log(`\n   Packets attempted: ${attempted}`);
        console.log(`   Packets sent: ${sent}`);
        console.log(`   Errors: ${errors}`);
        
        // Determine status
        console.log("\n" + "=".repeat(60));
        console.log("UWB STATUS SUMMARY");
        console.log("=".repeat(60));
        
        if (attempted > 0) {
          if (sent > 0) {
            console.log("✓ Node A is SENDING UWB packets!");
            console.log(`  Successfully sent ${sent} packets`);
            console.log("\n  Check Node B:");
            console.log("    - D13 LED should blink when receiving packets");
            console.log("    - If Node B D13 blinks, UWB communication is WORKING!");
          } else {
            console.log("⚠ Node A attempted to send but all packets failed");
            console.log("  Check:");
            console.log("    - Antenna connected?");
            console.log("    - DW3000 chip working?");
            console.log("    - Power supply stable?");
          }
        } else {
          console.log("✗ Node A is NOT sending packets");
          if (!isConfigured) {
            console.log("\n  Action needed:");
            console.log("    1. Configure: CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=10");
            console.log("    2. Start: STRT");
          } else {
            console.log("\n  Node A is configured but not started");
            console.log("  Action needed: Send STRT command to start transmission");
          }
        }
        
        // D13 LED interpretation
        console.log("\n" + "=".repeat(60));
        console.log("D13 LED INTERPRETATION");
        console.log("=".repeat(60));
        console.log("D13 (DW3000 internal LED) behavior:");
        console.log("  - Slow blink: DW3000 initialized (normal)");
        console.log("  - Fast blink: Packets being sent/received");
        console.log("  - Solid ON: Error state");
        console.log("  - OFF: DW3000 not initialized");
        console.log("\n  Your observation: 'D13 starts slow blinking with orange'");
        console.log("  → This means DW3000 is initialized and ready");
        console.log("  → If packets are being sent, D13 should blink faster");
        console.log("  → Check statistics above to confirm packets are being sent");
        
        console.log("\n" + "=".repeat(60));
        
        serialPort.close();
        process.exit(0);
        
      } catch (error) {
        console.error("Error:", error.message);
        serialPort.close();
        process.exit(1);
      }
    })();
  }, 3000);
});

serialPort.open((err) => {
  if (err) {
    console.error(`[ERROR] Failed to open port: ${err.message}`);
    console.error("\nPort may be in use. Try:");
    console.error("  - Closing other programs using the port");
    console.error("  - Using a different port");
    process.exit(1);
  }
});

