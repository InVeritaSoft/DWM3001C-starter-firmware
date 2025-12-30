#!/usr/bin/env node
/**
 * Check Node A (TX) Status
 * Diagnoses why Node A isn't sending UWB packets
 * 
 * Usage: node scripts/check-node-a-status.js <COM_PORT>
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2];

if (!portName) {
  console.error("Usage: node scripts/check-node-a-status.js <COM_PORT>");
  console.error("Example: node scripts/check-node-a-status.js /dev/ttyUSB0");
  process.exit(1);
}

console.log("=".repeat(60));
console.log("NODE A (TX) STATUS CHECK");
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

let allResponses = [];
let responseBuffer = [];

parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  if (trimmed.length > 0) {
    allResponses.push(trimmed);
    responseBuffer.push(trimmed);
    console.log(`[RX] ${trimmed}`);
  }
});

serialPort.on("open", () => {
  console.log("✓ Port opened\n");
  console.log("Waiting 3 seconds for startup messages...\n");
  
  setTimeout(() => {
    responseBuffer = []; // Clear startup messages
    
    const sendCommand = (cmd) => {
      return new Promise((resolve) => {
        // Clear buffer before sending command
        responseBuffer = [];
        
        // Set up timeout
        const timeout = setTimeout(() => {
          // Filter out startup messages
          const filtered = responseBuffer.filter(r => 
            !r.includes("STARTUP") && 
            !r.includes("DW3000_READY") && 
            !r.includes("MAIN_LOOP")
          );
          resolve(filtered);
        }, 2000);
        
        // Send command
        const fullCmd = cmd + "\r\n";
        serialPort.write(fullCmd, (err) => {
          if (err) {
            clearTimeout(timeout);
            resolve([]);
          } else {
            console.log(`[TX] ${cmd}`);
          }
        });
      });
    };
    
    (async () => {
      try {
        console.log("=".repeat(60));
        console.log("STEP 1: Check Node Type");
        console.log("=".repeat(60));
        console.log("Sending NODE_TYPE command...");
        console.log("Watch Orange LED (D10) - should blink if command received!\n");
        const nodeTypeResp = await sendCommand("NODE_TYPE");
        console.log("Raw responses received:", nodeTypeResp);
        if (nodeTypeResp.length === 0) {
          console.log("\n❌ NO RESPONSES RECEIVED!");
          console.log("This means RS-485 communication is not working.");
          console.log("\nCheck:");
          console.log("  1. Is RS-485 DE/RE pin connected? (GPIO P0.06)");
          console.log("  2. Is RS-485 converter powered?");
          console.log("  3. Are RO/DI pins connected correctly?");
          console.log("  4. Watch Orange LED - does it blink when command sent?");
          console.log("\nTry: npm run test-serial -- " + portName);
          serialPort.close();
          process.exit(1);
        }
        if (nodeTypeResp.some(r => r.includes("TX"))) {
          console.log("✓ Node A (TX) confirmed\n");
        } else {
          console.log("⚠ Node type response:", nodeTypeResp);
          console.log("(May be Node B or different firmware)\n");
        }
        
        console.log("=".repeat(60));
        console.log("STEP 2: Check Configuration");
        console.log("=".repeat(60));
        const configResp = await sendCommand("CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=10");
        if (configResp.some(r => r.includes("OK CONFIG"))) {
          console.log("✓ Node A configured\n");
        } else {
          console.log("⚠ Configuration response:", configResp);
          console.log("(May already be configured)\n");
        }
        
        console.log("=".repeat(60));
        console.log("STEP 3: Check Statistics (Before Start)");
        console.log("=".repeat(60));
        const statsBefore = await sendCommand("STAT");
        console.log("Stats:", statsBefore.join(", "));
        console.log("");
        
        console.log("=".repeat(60));
        console.log("STEP 4: Start Node A");
        console.log("=".repeat(60));
        console.log("Sending START command...");
        console.log("Watch D13 LED on Node A - should blink when sending packets!\n");
        const startResp = await sendCommand("STRT");
        if (startResp.some(r => r.includes("OK START"))) {
          console.log("✓ Node A started\n");
        } else if (startResp.some(r => r.includes("ERR NOT_CONFIGURED"))) {
          console.log("❌ Node A not configured! Run CFG command first.\n");
        } else if (startResp.some(r => r.includes("ERR TIMER_NOT_INIT"))) {
          console.log("❌ Timer not initialized! Firmware error.\n");
        } else {
          console.log("⚠ Start response:", startResp);
          console.log("(May have started anyway)\n");
        }
        
        console.log("=".repeat(60));
        console.log("STEP 5: Wait and Check Statistics (After Start)");
        console.log("=".repeat(60));
        console.log("Waiting 5 seconds for packets to be sent...\n");
        await new Promise(resolve => setTimeout(resolve, 5000));
        
        const statsAfter = await sendCommand("STAT");
        console.log("Stats:", statsAfter.join(", "));
        console.log("");
        
        // Parse stats
        const statsStr = statsAfter.join(" ");
        const sentMatch = statsStr.match(/sent=(\d+)/);
        const attemptedMatch = statsStr.match(/attempted=(\d+)/);
        const errorsMatch = statsStr.match(/errors=(\d+)/);
        const timeoutsMatch = statsStr.match(/timeouts=(\d+)/);
        
        const sent = sentMatch ? parseInt(sentMatch[1]) : 0;
        const attempted = attemptedMatch ? parseInt(attemptedMatch[1]) : 0;
        const errors = errorsMatch ? parseInt(errorsMatch[1]) : 0;
        const timeouts = timeoutsMatch ? parseInt(timeoutsMatch[1]) : 0;
        
        console.log("=".repeat(60));
        console.log("DIAGNOSIS");
        console.log("=".repeat(60));
        console.log(`Packets attempted: ${attempted}`);
        console.log(`Packets sent: ${sent}`);
        console.log(`TX errors: ${errors}`);
        console.log(`TX timeouts: ${timeouts}`);
        console.log("");
        
        if (attempted === 0) {
          console.log("❌ PROBLEM: No packets attempted!");
          console.log("This means:");
          console.log("  1. START command didn't work");
          console.log("  2. Timer not running");
          console.log("  3. Test not started");
          console.log("");
          console.log("Check:");
          console.log("  - Did START command return 'OK START'?");
          console.log("  - Is Node A configured? (CFG command)");
          console.log("  - Check for timer initialization errors");
        } else if (sent === 0 && attempted > 0) {
          console.log("❌ PROBLEM: Packets attempted but none sent!");
          console.log("This means:");
          console.log("  1. TX errors or timeouts");
          console.log("  2. DW3000 chip not responding");
          console.log("  3. Antenna not connected");
          console.log("");
          console.log("Check:");
          console.log("  - Antenna connected?");
          console.log("  - DW3000 chip working? (D13 should blink)");
          console.log("  - Check TX errors/timeouts above");
        } else if (sent > 0) {
          console.log("✓ SUCCESS: Node A is sending packets!");
          console.log(`  Sent ${sent} packets successfully`);
          console.log("");
          console.log("If D13 doesn't blink on Node A:");
          console.log("  - DW3000 LED might be disabled");
          console.log("  - But packets are being sent (check Node B D13)");
        }
        
        console.log("\n" + "=".repeat(60));
        console.log("Test complete. Press Ctrl+C to exit.");
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
  console.log("\n\nClosing port...");
  serialPort.close();
  process.exit(0);
});
