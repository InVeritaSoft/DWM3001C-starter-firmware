import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import yaml from "js-yaml";
import { getConfig as getEnvConfig } from "../../scripts/load-env.js";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

/**
 * Configuration Manager
 * Loads settings from YAML/JSON files
 */
export class Config {
  constructor() {
    this.settings = null;
    this.testPlan = null;
  }

  /**
   * Load settings from YAML file
   * @param {string} settingsPath - Path to settings.yaml
   */
  loadSettings(
    settingsPath = path.join(__dirname, "../../config/settings.yaml"),
  ) {
    try {
      if (!fs.existsSync(settingsPath)) {
        console.warn(
          `Settings file not found: ${settingsPath}, using defaults`,
        );
        this.settings = this.getDefaultSettings();
        return this.settings;
      }
      const fileContents = fs.readFileSync(settingsPath, "utf8");
      this.settings = yaml.load(fileContents);
      return this.settings;
    } catch (error) {
      console.warn(`Failed to load settings: ${error.message}, using defaults`);
      this.settings = this.getDefaultSettings();
      return this.settings;
    }
  }

  /**
   * Load test plan from JSON file
   * @param {string} testPlanPath - Path to testPlan.json
   */
  loadTestPlan(
    testPlanPath = path.join(__dirname, "../../config/testPlan.json"),
  ) {
    try {
      if (!fs.existsSync(testPlanPath)) {
        console.warn(
          `Test plan file not found: ${testPlanPath}, using empty test plan`,
        );
        this.testPlan = { tests: [] };
        return this.testPlan;
      }
      const fileContents = fs.readFileSync(testPlanPath, "utf8");
      this.testPlan = JSON.parse(fileContents);
      return this.testPlan;
    } catch (error) {
      console.warn(
        `Failed to load test plan: ${error.message}, using empty test plan`,
      );
      this.testPlan = { tests: [] };
      return this.testPlan;
    }
  }

  /**
   * Get default settings
   * @returns {Object}
   */
  getDefaultSettings() {
    // Communication mode: "jlink" (J-Link CDC UART) or "rs485" (CH340 USB-to-RS485)
    const commMode = (process.env.COMM_MODE || "jlink").toLowerCase();

    // Default ports based on comm mode
    const defaultPorts =
      commMode === "rs485"
        ? { node_a: "COM20", node_b: "COM19" } // CH340 RS-485 adapters
        : { node_a: "COM15", node_b: "COM11" }; // J-Link CDC UART

    return {
      serial: {
        node_a_port:
          process.env.SERIAL_NODE_A_PORT ||
          process.env.NODE_A_PORT ||
          defaultPorts.node_a,
        node_b_port:
          process.env.SERIAL_NODE_B_PORT ||
          process.env.NODE_B_PORT ||
          defaultPorts.node_b,
        baudrate: parseInt(
          process.env.SERIAL_BAUDRATE || process.env.BAUDRATE || "115200",
          10,
        ),
        timeout: parseInt(
          process.env.SERIAL_TIMEOUT || process.env.TIMEOUT || "5",
          10,
        ),
        comm_mode: commMode,
      },
      test: {
        poll_interval_seconds: 0.5, // 500ms for real-time chart updates
        default_test_duration_seconds: 30,
        channel: 5,
        data_rate: "6m8",
        preamble_len: 128,
        payload_len: 64,
        tx_power_idx: 5,
        pkt_rate_hz: 100,
      },
      web: {
        // Force port 5000 for backend API (frontend dev server uses 3000 and proxies to 5000)
        port: parseInt(process.env.WEB_PORT || process.env.PORT || "5000", 10),
        host: process.env.WEB_HOST || process.env.HOST || "0.0.0.0",
      },
      swagger: {
        title: "UWB Test Orchestrator API",
        version: "1.0.0",
        description: "API for controlling UWB DWM3001C test rig",
      },
    };
  }

  /**
   * Get serial port configuration
   * Prioritizes .env file over settings.yaml
   * @returns {Object}
   */
  getSerialConfig() {
    // Try to load from .env first (single source of truth)
    try {
      const envConfig = getEnvConfig();
      if (envConfig && envConfig.serial && envConfig.serial.node_a_port) {
        return envConfig.serial;
      }
    } catch (error) {
      // .env not available, fall back to YAML
    }

    // Fall back to YAML settings
    if (!this.settings) {
      this.loadSettings();
    }
    return this.settings.serial || {};
  }

  /**
   * Get test configuration
   * Prioritizes .env file over settings.yaml
   * @returns {Object}
   */
  getTestConfig() {
    // Try to load from .env first
    try {
      const envConfig = getEnvConfig();
      if (envConfig && envConfig.test) {
        return envConfig.test;
      }
    } catch (error) {
      // .env not available, fall back to YAML
    }

    // Fall back to YAML settings
    if (!this.settings) {
      this.loadSettings();
    }
    return this.settings.test || {};
  }

  /**
   * Get web server configuration
   * Prioritizes .env file over settings.yaml
   * @returns {Object}
   */
  getWebConfig() {
    // Try to load from .env first
    try {
      const envConfig = getEnvConfig();
      if (envConfig && envConfig.web) {
        return envConfig.web;
      }
    } catch (error) {
      // .env not available, fall back to YAML
    }

    // Fall back to YAML settings
    if (!this.settings) {
      this.loadSettings();
    }
    return this.settings.web || {};
  }

  /**
   * Get Swagger configuration
   * @returns {Object}
   */
  getSwaggerConfig() {
    if (!this.settings) {
      this.loadSettings();
    }
    return this.settings.swagger || {};
  }

  /**
   * Get test plan
   * @returns {Array}
   */
  getTestPlan() {
    if (!this.testPlan) {
      this.loadTestPlan();
    }
    return this.testPlan.tests || [];
  }

  /**
   * Get default UWB configuration
   * @returns {Object}
   */
  getDefaultUWBConfig() {
    return {
      channel: 5,
      data_rate: "6m8",
      preamble_len: 128,
      payload_len: 64,
      tx_power_idx: 5,
      pkt_rate_hz: 100,
    };
  }
}

// Singleton instance
let configInstance = null;

/**
 * Get configuration instance
 * @returns {Config}
 */
export function getConfig() {
  if (!configInstance) {
    configInstance = new Config();
  }
  return configInstance;
}
