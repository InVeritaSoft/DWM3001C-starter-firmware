#!/usr/bin/env node
/**
 * Check what baud rate the orchestrator is actually using
 */

import { Config } from "../src/orchestrator/config.js";
import { getConfig as getEnvConfig } from "./load-env.js";

console.log("=".repeat(60));
console.log("BAUD RATE CHECK");
console.log("=".repeat(60));
console.log("");

const config = new Config();
config.loadSettings();

// Load environment overrides
const envConfig = getEnvConfig();
const serialConfig = config.getSerialConfig();

console.log("Configuration Sources:");
console.log("  1. settings.yaml:");
console.log(`     baudrate: ${config.settings?.serial?.baudrate || 'not set'}`);
console.log("");
console.log("  2. Environment variables:");
console.log(`     SERIAL_BAUDRATE: ${process.env.SERIAL_BAUDRATE || 'not set'}`);
console.log(`     BAUDRATE: ${process.env.BAUDRATE || 'not set'}`);
console.log("");
console.log("  3. Final resolved value:");
console.log(`     baudrate: ${serialConfig.baudrate}`);
console.log("");
console.log("  4. Ports:");
console.log(`     node_a_port: ${serialConfig.node_a_port}`);
console.log(`     node_b_port: ${serialConfig.node_b_port}`);
console.log("");

if (serialConfig.baudrate !== 57600) {
  console.log("⚠️  WARNING: Baud rate is NOT 57600!");
  console.log(`   Current: ${serialConfig.baudrate}`);
  console.log(`   Expected: 57600 (matches Arduino firmware)`);
  console.log("");
  console.log("To fix:");
  console.log("  1. Set environment variable: export BAUDRATE=57600");
  console.log("  2. Or edit config/settings.yaml: baudrate: 57600");
  console.log("  3. Restart orchestrator");
} else {
  console.log("✓ Baud rate is correctly set to 57600");
}

console.log("");
