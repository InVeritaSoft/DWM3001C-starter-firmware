#!/usr/bin/env node
/**
 * RS-485 Hardware Diagnostic Script
 * Checks if RS-485 hardware is properly connected and configured
 * 
 * Usage: node scripts/check-rs485-hardware.js <COM_PORT>
 * Example: node scripts/check-rs485-hardware.js /dev/ttyUSB0
 */

import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

const portName = process.argv[2];

if (!portName) {
  console.error("Usage: node scripts/check-rs485-hardware.js <COM_PORT>");
  console.error("Example: node scripts/check-rs485-hardware.js /dev/ttyUSB0");
  process.exit(1);
}

console.log("=".repeat(60));
console.log("RS-485 HARDWARE DIAGNOSTIC");
console.log("=".repeat(60));
console.log(`Port: ${portName}`);
console.log(`Baud: 115200`);
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
let rawData = [];

parser.on("data", (data) => {
  const trimmed = data.toString().trim();
  responses.push(trimmed);
  console.log(`[RX] ${trimmed}`);
});

serialPort.on("data", (data) => {
  rawData.push(data.toString("hex"));
});

serialPort.on("error", (error) => {
  console.error(`[ERROR] ${error.message}`);
});

serialPort.on("open", () => {
  console.log("✓ Port opened\n");
  
  console.log("=".repeat(60));
  console.log("CRITICAL CHECKS");
  console.log("=".repeat(60));
  console.log("");
  console.log("1. RS-485 DE/RE PIN CONNECTION:");
  console.log("   ⚠️  CRITICAL: DE/RE must be connected to GPIO P0.06");
  console.log("   - Measure voltage on P0.06 (should be LOW/0V when idle)");
  console.log("   - If DE/RE is not connected, transceiver won't receive commands");
  console.log("   - Check: Is DE/RE wire connected from board to RS-485 converter?");
  console.log("");
  console.log("2. RS-485 CONVERTER POWER:");
  console.log("   - Is converter powered? (Check power LED if available)");
  console.log("   - Measure VCC voltage (should be 3.3V or 5V)");
  console.log("   - Check GND connection");
  console.log("");
  console.log("3. RS-485 WIRING:");
  console.log("   - RO (Receive Out) → GPIO 15 (Pin 10 on J10)");
  console.log("   - DI (Data In) ← GPIO 14 (Pin 8 on J10)");
  console.log("   - GND → Pin 6 on J10");
  console.log("   - DE/RE → GPIO P0.06");
  console.log("   - A+/B- lines connected correctly");
  console.log("");
  console.log("4. SERIAL PORT:");
  console.log(`   - Port ${portName} is correct`);
  console.log("   - No other software using the port");
  console.log("   - Baud rate matches (115200)");
  console.log("");
  
  setTimeout(() => {
    console.log("=".repeat(60));
    console.log("TESTING COMMUNICATION");
    console.log("=".repeat(60));
    console.log("");
    console.log("Sending PING command...");
    console.log("Watch Orange LED (D10) - should blink if command received!");
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
      console.log("RESULTS");
      console.log("=".repeat(60));
      
      if (responses.length === 0) {
        console.log("❌ NO RESPONSES RECEIVED!");
        console.log("");
        console.log("This means commands are NOT reaching the firmware.");
        console.log("");
        console.log("MOST LIKELY CAUSES:");
        console.log("");
        console.log("1. DE/RE PIN NOT CONNECTED (90% probability)");
        console.log("   - RS-485 transceiver DE/RE must be connected to GPIO P0.06");
        console.log("   - Without DE/RE, transceiver is stuck in wrong mode");
        console.log("   - Check: Is there a wire from board P0.06 to converter DE/RE?");
        console.log("");
        console.log("2. RS-485 CONVERTER NOT POWERED");
        console.log("   - Converter needs power to function");
        console.log("   - Check power LED or measure VCC voltage");
        console.log("");
        console.log("3. WRONG WIRING");
        console.log("   - RO → GPIO 15 (Pin 10 on J10)");
        console.log("   - DI ← GPIO 14 (Pin 8 on J10)");
        console.log("   - Verify with multimeter continuity test");
        console.log("");
        console.log("4. WRONG SERIAL PORT");
        console.log(`   - Verify ${portName} is the correct port`);
        console.log("   - Check: ls -l /dev/ttyUSB* or ls -l /dev/ttyACM*");
        console.log("");
        console.log("IMMEDIATE ACTIONS:");
        console.log("1. Check DE/RE connection: Board P0.06 → Converter DE/RE");
        console.log("2. Measure P0.06 voltage (should be LOW/0V when idle)");
        console.log("3. Verify converter is powered");
        console.log("4. Test with multimeter: continuity between board pins and converter");
        console.log("5. Try direct USB connection to verify firmware works");
      } else {
        console.log(`✓ Received ${responses.length} response(s):`);
        responses.forEach((resp, idx) => {
          console.log(`   ${idx + 1}. ${resp}`);
        });
        console.log("");
        console.log("✓ Communication is working!");
        console.log("If Orange LED didn't blink, check LED wiring or firmware version.");
      }
      
      console.log("\n" + "=".repeat(60));
      console.log("Test complete. Press Ctrl+C to exit.");
      console.log("=".repeat(60));
    }, 2000);
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
  serialPort.close((err) => {
    if (err) {
      console.error(`[ERROR] Failed to close port: ${err.message}`);
    } else {
      console.log("Port closed.");
    }
    process.exit(0);
  });
});

