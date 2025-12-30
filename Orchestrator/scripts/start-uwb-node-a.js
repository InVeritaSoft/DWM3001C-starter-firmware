#!/usr/bin/env node
/**
 * Start UWB Communication - Node A Only
 * Configures and starts Node A to send UWB packets to Node B
 * Node B should already be in RX mode (enabled at startup)
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2] || "/dev/ttyUSB0";

console.log("=".repeat(60));
console.log("START UWB COMMUNICATION - NODE A");
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
let allData = [];
let startupMessages = [];

// Also capture raw data to see if anything is coming through
serialPort.on("data", (data) => {
  const hex = data.toString("hex");
  if (hex.length > 0) {
    allData.push(`Raw: ${hex}`);
  }
});

parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  if (trimmed.length > 0) {
    if (trimmed.includes("STARTUP") || trimmed.includes("DW3000_READY") || trimmed.includes("MAIN_LOOP")) {
      startupMessages.push(trimmed);
      console.log(`[STARTUP] ${trimmed}`);
    } else {
      responses.push(trimmed);
      console.log(`[RX] ${trimmed}`);
    }
  }
});

serialPort.on("open", () => {
  console.log("✓ Port opened\n");
  console.log("Waiting 3 seconds for startup messages...\n");
  console.log("Watch Node A LEDs:");
  console.log("  - D10 (Orange) should blink when command is received");
  console.log("  - D11 (Green) should blink when response is sent");
  console.log("  - D13 (DW3000) slow blink = initialized\n");
  
  setTimeout(() => {
    console.log("\n=== Startup Messages ===");
    if (startupMessages.length > 0) {
      console.log(`✓ Received ${startupMessages.length} startup message(s)`);
      startupMessages.forEach(msg => console.log(`   ${msg}`));
    } else {
      console.log("⚠ No startup messages received");
      console.log("   This may indicate:");
      console.log("   - Firmware not running");
      console.log("   - RS-485 not receiving data");
      console.log("   - Wrong port");
    }
    console.log("");
    
    responses = [];
    allData = [];
    
    const sendCommand = (cmd, waitTime = 3000) => {
      return new Promise((resolve) => {
        responses = [];
        allData = [];
        const timeout = setTimeout(() => {
          console.log(`[TIMEOUT] No response after ${waitTime}ms`);
          if (allData.length > 0) {
            console.log(`[DEBUG] Raw data received: ${allData.join(", ")}`);
          }
          resolve(responses);
        }, waitTime);
        
        const fullCmd = cmd + "\r\n";
        const hex = Buffer.from(fullCmd).toString("hex");
        serialPort.write(fullCmd, (err) => {
          if (!err) {
            console.log(`[TX] ${cmd}`);
            console.log(`[TX] Hex: ${hex}`);
            console.log("   → Watch D10 (Orange) LED - should blink if command received!");
          } else {
            console.error(`[ERROR] Write failed: ${err.message}`);
            clearTimeout(timeout);
            resolve([]);
          }
        });
      });
    };
    
    (async () => {
      try {
        // Step 1: Verify Node A
        console.log("=".repeat(60));
        console.log("STEP 1: Verify Node A");
        console.log("=".repeat(60));
        console.log("Sending NODE_TYPE command...");
        console.log("→ Did D10 (Orange) blink? If NO, commands not reaching firmware!\n");
        const nodeType = await sendCommand("NODE_TYPE");
        if (nodeType.some(r => r.includes("TX"))) {
          console.log("✓ Node A (TX) confirmed\n");
        } else {
          console.log("⚠ Node type response:", nodeType.length > 0 ? nodeType : "NO RESPONSE");
          if (nodeType.length === 0) {
            console.log("\n❌ PROBLEM: No response from Node A!");
            console.log("   Check:");
            console.log("   1. Did D10 (Orange) blink? → If NO: Commands not reaching firmware");
            console.log("   2. Did D11 (Green) blink? → If NO: Responses not being sent");
            console.log("   3. RS-485 DE/RE pin (P0.06) connected?");
            console.log("   4. RS-485 wiring correct?");
            console.log("\n   This is an RS-485 communication issue, not UWB issue.\n");
          }
        }
        
        // Step 2: Configure Node A
        console.log("=".repeat(60));
        console.log("STEP 2: Configure Node A");
        console.log("=".repeat(60));
        console.log("Configuring UWB settings...");
        console.log("→ Watch D10 (Orange) LED\n");
        const config = await sendCommand("CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=10");
        if (config.some(r => r.includes("OK CONFIG"))) {
          console.log("✓ Node A configured\n");
        } else {
          console.log("⚠ Config response:", config.length > 0 ? config : "NO RESPONSE");
          if (config.length === 0) {
            console.log("\n❌ Cannot configure - no response from Node A");
            console.log("   Fix RS-485 communication first!\n");
          }
        }
        
        // Step 3: Start Node A
        console.log("=".repeat(60));
        console.log("STEP 3: Start Node A - UWB Transmission");
        console.log("=".repeat(60));
        console.log("Starting UWB transmission...");
        console.log("Watch D13 LED on Node A - should blink when sending packets!");
        console.log("Watch D13 LED on Node B - should blink when receiving packets!\n");
        const start = await sendCommand("STRT");
        if (start.some(r => r.includes("OK START"))) {
          console.log("✓ Node A started - sending UWB packets!\n");
        } else {
          console.log("⚠ Start response:", start);
          if (start.some(r => r.includes("NOT_CONFIGURED"))) {
            console.log("❌ Node A not configured! Configuration failed.");
            serialPort.close();
            process.exit(1);
          }
        }
        
        // Step 4: Check statistics
        console.log("=".repeat(60));
        console.log("STEP 4: Check Statistics");
        console.log("=".repeat(60));
        console.log("Waiting 5 seconds for packets to be sent...\n");
        await new Promise(resolve => setTimeout(resolve, 5000));
        
        const stats = await sendCommand("STAT");
        console.log("Statistics:", stats.join(", "));
        
        // Parse stats
        const statsStr = stats.join(" ");
        const sentMatch = statsStr.match(/sent=(\d+)/);
        const attemptedMatch = statsStr.match(/attempted=(\d+)/);
        
        const sent = sentMatch ? parseInt(sentMatch[1]) : 0;
        const attempted = attemptedMatch ? parseInt(attemptedMatch[1]) : 0;
        
        console.log("\n" + "=".repeat(60));
        console.log("UWB COMMUNICATION STATUS");
        console.log("=".repeat(60));
        console.log(`Packets attempted: ${attempted}`);
        console.log(`Packets sent: ${sent}`);
        console.log("");
        
        if (sent > 0) {
          console.log("✓ SUCCESS: Node A is sending UWB packets!");
          console.log(`  Sent ${sent} packets successfully`);
          console.log("");
          console.log("Check Node B:");
          console.log("  - D13 LED should blink when receiving packets");
          console.log("  - If D13 blinks, UWB communication is working!");
        } else if (attempted > 0) {
          console.log("⚠ Node A attempted to send but packets failed");
          console.log("Check:");
          console.log("  - Antenna connected?");
          console.log("  - DW3000 chip working?");
        } else {
          console.log("❌ No packets attempted");
          if (config.length === 0 || start.length === 0) {
            console.log("\n⚠ Node A never received configuration/start commands");
            console.log("   This is an RS-485 communication problem:");
            console.log("   - Commands sent from RPi5");
            console.log("   - But responses not received by RPi5");
            console.log("   - Check RS-485 DE/RE pin (P0.06) connection");
            console.log("   - Check RS-485 transceiver wiring");
          } else {
            console.log("Node A may not have started properly");
          }
        }
        
        console.log("\n" + "=".repeat(60));
        console.log("UWB communication test running...");
        console.log("Press Ctrl+C to stop.");
        console.log("=".repeat(60));
        
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
    process.exit(1);
  }
});

process.on("SIGINT", () => {
  console.log("\n\nStopping Node A...");
  responses = [];
  serialPort.write("STOP\r\n", () => {
    setTimeout(() => {
      console.log("Closing port...");
      serialPort.close();
      process.exit(0);
    }, 1000);
  });
});

