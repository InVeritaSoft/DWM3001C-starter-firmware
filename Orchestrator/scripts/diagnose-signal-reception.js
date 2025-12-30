#!/usr/bin/env node
/**
 * Signal Reception Diagnostic
 * Tests if signals are reaching GPIO but firmware isn't processing them
 * 
 * Usage: node scripts/diagnose-signal-reception.js <COM_PORT>
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2];

if (!portName) {
  console.error("Usage: node scripts/diagnose-signal-reception.js <COM_PORT>");
  process.exit(1);
}

console.log("=".repeat(60));
console.log("SIGNAL RECEPTION DIAGNOSTIC");
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
let rawBytes = 0;

parser.on("data", (data) => {
  responses.push(data.toString().trim());
  console.log(`[RX] ${data.toString().trim()}`);
});

serialPort.on("data", (data) => {
  rawBytes += data.length;
});

serialPort.on("open", () => {
  console.log("✓ Port opened\n");
  
  console.log("=".repeat(60));
  console.log("CRITICAL: RS-485 DE/RE PIN CHECK");
  console.log("=".repeat(60));
  console.log("");
  console.log("⚠️  MOST LIKELY ISSUE: DE/RE pin not connected!");
  console.log("");
  console.log("If signals reach GPIO but firmware doesn't respond:");
  console.log("1. RS-485 transceiver DE/RE pin MUST be connected to GPIO P0.06");
  console.log("2. Without DE/RE, transceiver is stuck in TX mode (can't receive)");
  console.log("3. Check: Is there a wire from board P0.06 to converter DE/RE?");
  console.log("");
  console.log("Measure P0.06 voltage:");
  console.log("  - Should be LOW (0V) when idle = RX mode");
  console.log("  - If HIGH (3.3V) or floating = transceiver in TX mode = can't receive!");
  console.log("");
  
  setTimeout(() => {
    console.log("=".repeat(60));
    console.log("SENDING TEST COMMANDS");
    console.log("=".repeat(60));
    console.log("");
    console.log("Sending 10 PING commands rapidly...");
    console.log("Watch Orange LED (D10) - should blink for each command!");
    console.log("");
    
    let cmdCount = 0;
    const sendPing = () => {
      if (cmdCount < 10) {
        const pingCmd = "PNG\r\n";
        serialPort.write(pingCmd, (err) => {
          if (!err) {
            cmdCount++;
            console.log(`[TX ${cmdCount}/10] ${pingCmd.trim()}`);
          }
        });
        setTimeout(sendPing, 200);
      } else {
        setTimeout(() => {
          console.log("\n" + "=".repeat(60));
          console.log("RESULTS");
          console.log("=".repeat(60));
          console.log(`Commands sent: ${cmdCount}`);
          console.log(`Responses received: ${responses.length}`);
          console.log(`Raw bytes received: ${rawBytes}`);
          console.log("");
          
          if (responses.length === 0 && rawBytes === 0) {
            console.log("❌ NO DATA RECEIVED AT ALL!");
            console.log("");
            console.log("This means:");
            console.log("1. Signals are NOT reaching GPIO pins (despite what you think)");
            console.log("2. OR RS-485 transceiver is stuck in TX mode (DE/RE not connected)");
            console.log("3. OR UART is not initialized");
            console.log("");
            console.log("IMMEDIATE CHECKS:");
            console.log("");
            console.log("1. DE/RE PIN CONNECTION:");
            console.log("   - Board GPIO P0.06 → RS-485 converter DE/RE pin");
            console.log("   - Measure P0.06 voltage (should be LOW/0V)");
            console.log("   - If HIGH or floating, transceiver can't receive!");
            console.log("");
            console.log("2. SIGNAL LEVELS:");
            console.log("   - GPIO pins are 3.3V logic");
            console.log("   - RS-485 converter TTL I/O must be 3.3V compatible");
            console.log("   - If converter outputs 5V, it won't work!");
            console.log("");
            console.log("3. WIRING VERIFICATION:");
            console.log("   - RO (converter) → GPIO 15 (Pin 10 on J10)");
            console.log("   - DI (converter) ← GPIO 14 (Pin 8 on J10)");
            console.log("   - GND connected");
            console.log("   - Test continuity with multimeter");
            console.log("");
            console.log("4. CONVERTER POWER:");
            console.log("   - Converter must be powered (3.3V or 5V)");
            console.log("   - Check power LED or measure VCC");
            console.log("");
            console.log("5. BAUD RATE:");
            console.log("   - Must be exactly 115200");
            console.log("   - Check converter and sender baud rate match");
            console.log("");
          } else if (responses.length === 0 && rawBytes > 0) {
            console.log("⚠️  Raw data received but not parsed!");
            console.log("This suggests baud rate mismatch or data corruption.");
            console.log("Check baud rate is exactly 115200.");
          } else if (responses.length > 0) {
            console.log("✓ Communication is working!");
            console.log("If Orange LED didn't blink, check LED wiring.");
          }
          
          console.log("\n" + "=".repeat(60));
          console.log("Test complete.");
          console.log("=".repeat(60));
        }, 3000);
      }
    };
    
    sendPing();
  }, 2000);
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

