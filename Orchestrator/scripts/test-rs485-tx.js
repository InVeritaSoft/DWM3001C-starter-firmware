#!/usr/bin/env node
/**
 * Test RS485 Transmission
 * Sends test commands and verifies data is actually being transmitted
 * Usage: node scripts/test-rs485-tx.js <PORT> [BAUDRATE]
 */

import { SerialPort } from "serialport";

const portName = process.argv[2];
const baudrate = parseInt(process.argv[3] || "57600", 10);

if (!portName) {
  console.error("Usage: node scripts/test-rs485-tx.js <PORT> [BAUDRATE]");
  console.error("Example: node scripts/test-rs485-tx.js /dev/ttyUSB0 57600");
  process.exit(1);
}

console.log("=".repeat(60));
console.log("RS485 TRANSMISSION TEST");
console.log("=".repeat(60));
console.log(`Port: ${portName}`);
console.log(`Baudrate: ${baudrate}`);
console.log("");

const serialPort = new SerialPort({
  path: portName,
  baudRate: baudrate,
  dataBits: 8,
  parity: "none",
  stopBits: 1,
  autoOpen: false,
});

let bytesSent = 0;
let testCount = 0;

serialPort.on("open", () => {
  console.log("✓ Serial port opened");
  console.log("");
  console.log("Sending test commands...");
  console.log("Watch Arduino Serial Monitor for incoming data");
  console.log("");

  // Send test commands every 2 seconds
  const sendTest = () => {
    testCount++;
    const testCmd = `PNG\r\n`;
    const hex = Buffer.from(testCmd).toString("hex");
    
    console.log(`[TEST ${testCount}] Sending: "${testCmd.trim()}"`);
    console.log(`[TEST ${testCount}] Hex: ${hex}`);
    console.log(`[TEST ${testCount}] Bytes: ${testCmd.length} (${Array.from(Buffer.from(testCmd)).map(b => `0x${b.toString(16).toUpperCase().padStart(2, '0')}`).join(', ')})`);
    
    serialPort.write(testCmd, (err) => {
      if (err) {
        console.error(`[ERROR] Write failed: ${err.message}`);
      } else {
        bytesSent += testCmd.length;
        console.log(`[TEST ${testCount}] ✓ Sent successfully (total bytes sent: ${bytesSent})`);
      }
    });
    
    console.log("");
    
    if (testCount < 10) {
      setTimeout(sendTest, 2000);
    } else {
      console.log("=".repeat(60));
      console.log("TEST COMPLETE");
      console.log("=".repeat(60));
      console.log(`Total commands sent: ${testCount}`);
      console.log(`Total bytes sent: ${bytesSent}`);
      console.log("");
      console.log("If Arduino shows 0 bytes received:");
      console.log("  1. Check RS485 wiring (A/B, GND, VCC)");
      console.log("  2. Verify MAX485 DE/RE pins connected to Arduino D2/D3");
      console.log("  3. Check RS485 converter power LED");
      console.log("  4. Verify baud rate matches (57600)");
      console.log("  5. Try swapping A/B lines");
      console.log("");
      setTimeout(() => {
        serialPort.close();
        process.exit(0);
      }, 1000);
    }
  };

  // Start sending after a short delay
  setTimeout(sendTest, 500);
});

serialPort.on("error", (err) => {
  console.error(`[ERROR] Serial port error: ${err.message}`);
  process.exit(1);
});

serialPort.on("close", () => {
  console.log("Serial port closed");
});

// Open the port
console.log("Opening serial port...");
serialPort.open((err) => {
  if (err) {
    console.error(`[ERROR] Failed to open port: ${err.message}`);
    console.error("");
    console.error("Common issues:");
    console.error("  1. Port doesn't exist - check: ls -l /dev/ttyUSB* or ls -l /dev/ttyACM*");
    console.error("  2. Port is in use by another program");
    console.error("  3. Permission denied - try: sudo chmod 666 /dev/ttyUSB*");
    process.exit(1);
  }
});
