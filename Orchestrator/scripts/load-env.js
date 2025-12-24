import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import os from "os";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

/**
 * Load environment variables from .env file
 * Checks for platform-specific files (.env.win on Windows, .env.linux on Linux)
 * Returns structured config object matching config.js expectations
 * @returns {Object|null} Config object or null if .env doesn't exist
 */
export function getConfig() {
  // Look for .env files in both Orchestrator directory and project root
  const orchestratorDir = path.join(__dirname, ".."); // Orchestrator/
  const projectRoot = path.join(__dirname, "../.."); // Project root
  const platform = os.platform();

  // Determine which .env file to use (platform-specific first, then generic)
  // Check project root first, then Orchestrator directory
  let envPath = null;
  if (platform === "win32") {
    // Check project root first: .env.win, then .env
    const envWinRoot = path.join(projectRoot, ".env.win");
    const envRoot = path.join(projectRoot, ".env");
    // Then check Orchestrator directory: .env.win, then .env
    const envWinOrch = path.join(orchestratorDir, ".env.win");
    const envOrch = path.join(orchestratorDir, ".env");

    if (fs.existsSync(envWinRoot)) {
      envPath = envWinRoot;
    } else if (fs.existsSync(envRoot)) {
      envPath = envRoot;
    } else if (fs.existsSync(envWinOrch)) {
      envPath = envWinOrch;
    } else if (fs.existsSync(envOrch)) {
      envPath = envOrch;
    }
  } else {
    // Linux/Mac: check for .env.linux or .env
    const envLinuxRoot = path.join(projectRoot, ".env.linux");
    const envRoot = path.join(projectRoot, ".env");
    const envLinuxOrch = path.join(orchestratorDir, ".env.linux");
    const envOrch = path.join(orchestratorDir, ".env");

    if (fs.existsSync(envLinuxRoot)) {
      envPath = envLinuxRoot;
    } else if (fs.existsSync(envRoot)) {
      envPath = envRoot;
    } else if (fs.existsSync(envLinuxOrch)) {
      envPath = envLinuxOrch;
    } else if (fs.existsSync(envOrch)) {
      envPath = envOrch;
    }
  }

  // If no .env file exists, return null (config.js will fall back to YAML)
  if (!envPath || !fs.existsSync(envPath)) {
    return null;
  }

  console.log(`[Config] Loading environment from: ${envPath}`);

  try {
    const envContent = fs.readFileSync(envPath, "utf8");
    const envVars = {};

    // Parse .env file (simple key=value format)
    envContent.split("\n").forEach((line) => {
      line = line.trim();
      // Skip empty lines and comments
      if (!line || line.startsWith("#")) {
        return;
      }

      const [key, ...valueParts] = line.split("=");
      if (key && valueParts.length > 0) {
        const value = valueParts.join("=").trim();
        // Remove quotes if present
        envVars[key.trim()] = value.replace(/^["']|["']$/g, "");
      }
    });

    // Build structured config object
    const config = {};

    // Serial configuration
    if (envVars.SERIAL_NODE_A_PORT || envVars.NODE_A_PORT) {
      // Parse timeout as float (supports values like 2.0)
      const timeoutValue = envVars.SERIAL_TIMEOUT || envVars.TIMEOUT || "5";
      const timeout = parseFloat(timeoutValue);

      config.serial = {
        node_a_port: envVars.SERIAL_NODE_A_PORT || envVars.NODE_A_PORT,
        node_b_port: envVars.SERIAL_NODE_B_PORT || envVars.NODE_B_PORT,
        baudrate: parseInt(
          envVars.SERIAL_BAUDRATE || envVars.BAUDRATE || "115200",
          10
        ),
        timeout: isNaN(timeout) ? 5 : timeout,
      };
    }

    // Test configuration
    if (
      envVars.TEST_CHANNEL ||
      envVars.TEST_DATA_RATE ||
      envVars.TEST_POLL_INTERVAL_SECONDS ||
      envVars.TEST_DEFAULT_DURATION_SECONDS
    ) {
      config.test = {};
      if (envVars.TEST_CHANNEL)
        config.test.channel = parseInt(envVars.TEST_CHANNEL, 10);
      if (envVars.TEST_DATA_RATE)
        config.test.data_rate = envVars.TEST_DATA_RATE;
      if (envVars.TEST_PREAMBLE_LEN)
        config.test.preamble_len = parseInt(envVars.TEST_PREAMBLE_LEN, 10);
      if (envVars.TEST_PAYLOAD_LEN)
        config.test.payload_len = parseInt(envVars.TEST_PAYLOAD_LEN, 10);
      if (envVars.TEST_TX_POWER_IDX)
        config.test.tx_power_idx = parseInt(envVars.TEST_TX_POWER_IDX, 10);
      if (envVars.TEST_PKT_RATE_HZ)
        config.test.pkt_rate_hz = parseInt(envVars.TEST_PKT_RATE_HZ, 10);
      if (envVars.TEST_POLL_INTERVAL_SECONDS)
        config.test.poll_interval_seconds = parseInt(
          envVars.TEST_POLL_INTERVAL_SECONDS,
          10
        );
      if (envVars.TEST_DEFAULT_DURATION_SECONDS)
        config.test.default_test_duration_seconds = parseInt(
          envVars.TEST_DEFAULT_DURATION_SECONDS,
          10
        );
    }

    // Web configuration
    if (envVars.WEB_PORT || envVars.PORT) {
      config.web = {
        port: parseInt(envVars.WEB_PORT || envVars.PORT || "3000", 10),
        host: envVars.WEB_HOST || envVars.HOST || "0.0.0.0",
      };
    }

    return Object.keys(config).length > 0 ? config : null;
  } catch (error) {
    console.warn(`Warning: Failed to load .env file: ${error.message}`);
    return null;
  }
}
