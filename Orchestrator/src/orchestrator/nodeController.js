import { EventEmitter } from "events";
import { RS485Comm } from "./rs485Comm.js";

/**
 * Node States
 */
export const NodeState = {
  IDLE: "IDLE",
  CONFIGURED: "CONFIGURED",
  RUNNING: "RUNNING",
  STOPPED: "STOPPED",
  ERROR: "ERROR",
};

/**
 * Node Controller
 * High-level control for UWB nodes (Node A = TX, Node B = RX)
 */
export class NodeController extends EventEmitter {
  constructor(nodeId, rs485Comm) {
    super();
    this.nodeId = nodeId; // 'A' or 'B'
    this.rs485Comm = rs485Comm;
    this.state = NodeState.IDLE;
    this.config = null;
    this.stats = null;
    this.lastError = null;

    // Bind RS-485 events
    this.rs485Comm.on("error", (error) => {
      this.setState(NodeState.ERROR);
      this.lastError = error.message;
      // Only emit error if there are listeners to prevent uncaught exceptions
      if (this.listenerCount("error") > 0) {
        this.emit("error", error);
      } else {
        // Log error if no listeners (prevents uncaught exception)
        console.error(
          `[NodeController ${this.nodeId}] Unhandled error:`,
          error.message,
        );
      }
    });

    this.rs485Comm.on("open", () => {
      this.emit("connected");
    });

    this.rs485Comm.on("close", () => {
      this.setState(NodeState.IDLE);
      this.emit("disconnected");
    });
  }

  /**
   * Set node state
   * @param {string} newState
   */
  setState(newState) {
    if (this.state !== newState) {
      const oldState = this.state;
      this.state = newState;
      this.emit("stateChange", { oldState, newState, nodeId: this.nodeId });
    }
  }

  /**
   * Get current state
   * @returns {string}
   */
  getState() {
    return this.state;
  }

  /**
   * Check if node is connected
   * @returns {boolean}
   */
  isConnected() {
    return this.rs485Comm.isOpen;
  }

  /**
   * Open connection to node
   */
  async connect() {
    try {
      await this.rs485Comm.open();
      return true;
    } catch (error) {
      this.setState(NodeState.ERROR);
      this.lastError = error.message;
      throw error;
    }
  }

  /**
   * Close connection to node
   */
  async disconnect() {
    try {
      await this.rs485Comm.close();
      this.setState(NodeState.IDLE);
      return true;
    } catch (error) {
      this.lastError = error.message;
      throw error;
    }
  }

  /**
   * Ping node to check connectivity
   * @returns {Promise<boolean>}
   */
  async ping() {
    try {
      // Use longer timeout for Node B (which is less reliable)
      const timeout = this.nodeId === "B" ? 8000 : 5000;

      const response = await this.rs485Comm.sendCommand("PNG", timeout);

      return response.startsWith("OK");
    } catch (error) {
      this.lastError = error.message;
      console.error(
        `⚠️  ${this.nodeId} PING failed - firmware may not be responding`,
      );
      console.error(`   Error: ${error.message.split("\n")[0]}`);
      if (error.message.includes("timeout")) {
        console.error(`   No response received. Check:`);
        console.error(`   1. Firmware is running orchestrator example`);
        console.error(`   2. RS-485 hardware connection`);
        console.error(`   3. Serial port ${this.rs485Comm.port} is correct`);
        console.error(
          `   4. Baud rate: Should be 115200 for RS485 communication (orchestrator to Arduino bridge)`,
        );
        console.error(
          `   5. If you see corrupted data (high-bit bytes), check baud rate mismatch`,
        );
        if (this.nodeId === "B") {
          console.error(
            `   6. Node B is less reliable - check RS-485 wiring and termination`,
          );
        }
      }
      return false;
    }
  }

  /**
   * Get firmware node type
   * @returns {Promise<string|null>}
   */
  async getFirmwareNodeType() {
    try {
      // Node B needs longer timeout
      const timeout = this.nodeId === "B" ? 8000 : 5000;
      const response = await this.rs485Comm.getNodeType(timeout);
      if (response && response.startsWith("OK NODE_TYPE=")) {
        const type = response.split("=")[1];
        const trimmedType = type.trim();
        return trimmedType;
      }
      return null;
    } catch (error) {
      this.lastError = error.message;
      return null;
    }
  }

  /**
   * Configure node with UWB settings
   * @param {Object} config - UWB configuration
   * @returns {Promise<boolean>}
   */
  async configure(config) {
    try {
      console.log(
        `[NodeController ${this.nodeId}] configure() called - state: ${this.state}, connected: ${this.isConnected()}`,
      );

      if (this.state === NodeState.RUNNING) {
        throw new Error("Cannot configure while test is running");
      }

      // Try to open connection if not already open
      if (!this.isConnected()) {
        console.log(
          `[NodeController ${this.nodeId}] Port not open, attempting to open...`,
        );
        try {
          await this.connect();
        } catch (connectError) {
          console.warn(
            `[NodeController ${this.nodeId}] Failed to open connection: ${connectError.message}, continuing anyway...`,
          );
        }
      }

      console.log(`[NodeController ${this.nodeId}] Sending configuration...`);
      // Configuration can take time - dwt_configure() and configure_tx_power() may take several seconds
      // Use longer timeout for configuration commands (20 seconds should be enough)
      const timeout = this.nodeId === "B" ? 20000 : 20000;
      const response = await this.rs485Comm.setConfig(config, timeout);
      console.log(
        `[NodeController ${this.nodeId}] Configuration response: ${response}`,
      );

      // Firmware responds with "OK CONFIG" (not "OK CONFIG_SET")
      if (response.startsWith("OK CONFIG")) {
        this.config = { ...config };
        this.setState(NodeState.CONFIGURED);
        this.emit("configured", config);
        const greenColor = "\x1b[32m";
        const resetColor = "\x1b[0m";
        console.log(
          `${greenColor}[NodeController ${this.nodeId}] ✓ Configuration successful${resetColor}`,
        );
        return true;
      } else {
        const errorMsg = `Configuration failed: ${response}`;
        console.error(`[NodeController ${this.nodeId}] ${errorMsg}`);
        throw new Error(errorMsg);
      }
    } catch (error) {
      console.error(
        `[NodeController ${this.nodeId}] configure() error: ${error.message}`,
      );
      this.setState(NodeState.ERROR);
      this.lastError = error.message;
      this.emit("error", error);
      throw error;
    }
  }

  /**
   * Start test
   * @returns {Promise<boolean>}
   */
  async startTest() {
    try {
      console.log(
        `[NodeController ${this.nodeId}] startTest() called - state: ${this.state}, connected: ${this.isConnected()}`,
      );

      // Try to open connection if not already open
      if (!this.isConnected()) {
        console.log(
          `[NodeController ${this.nodeId}] Port not open, attempting to open...`,
        );
        try {
          await this.connect();
        } catch (connectError) {
          console.warn(
            `[NodeController ${this.nodeId}] Failed to open connection: ${connectError.message}, continuing anyway...`,
          );
        }
      }

      console.log(`[NodeController ${this.nodeId}] Sending START command...`);
      // Node B needs longer timeout
      const timeout = this.nodeId === "B" ? 8000 : 5000;
      const response = await this.rs485Comm.startTest(timeout);
      console.log(
        `[NodeController ${this.nodeId}] START response: ${response}`,
      );

      // Firmware responds with "OK START" (not "OK TEST_STARTED")
      if (response.startsWith("OK START")) {
        this.setState(NodeState.RUNNING);
        this.emit("testStarted");
        const greenColor = "\x1b[32m";
        const resetColor = "\x1b[0m";
        console.log(
          `${greenColor}[NodeController ${this.nodeId}] ✓ Test started successfully${resetColor}`,
        );
        return true;
      } else {
        const errorMsg = `Start test failed: ${response}`;
        console.error(`[NodeController ${this.nodeId}] ${errorMsg}`);
        throw new Error(errorMsg);
      }
    } catch (error) {
      console.error(
        `[NodeController ${this.nodeId}] startTest() error: ${error.message}`,
      );
      this.setState(NodeState.ERROR);
      this.lastError = error.message;
      this.emit("error", error);
      throw error;
    }
  }

  /**
   * Stop test
   * @returns {Promise<boolean>}
   */
  async stopTest() {
    try {
      // Try to open connection if not already open
      if (!this.isConnected()) {
        console.log(
          `[NodeController ${this.nodeId}] Port not open, attempting to open...`,
        );
        try {
          await this.connect();
        } catch (connectError) {
          console.warn(
            `[NodeController ${this.nodeId}] Failed to open connection: ${connectError.message}, continuing anyway...`,
          );
        }
      }

      // Node B needs longer timeout
      const timeout = this.nodeId === "B" ? 8000 : 5000;
      const response = await this.rs485Comm.stopTest(timeout);
      if (response.startsWith("OK STOP")) {
        this.setState(NodeState.STOPPED);
        this.emit("testStopped");
        return true;
      } else {
        throw new Error(`Stop test failed: ${response}`);
      }
    } catch (error) {
      this.setState(NodeState.ERROR);
      this.lastError = error.message;
      this.emit("error", error);
      throw error;
    }
  }

  /**
   * Get statistics from node
   * @returns {Promise<Object>}
   */
  async getStats() {
    try {
      // Try to open connection if not already open
      if (!this.isConnected()) {
        console.log(
          `[NodeController ${this.nodeId}] Port not open, attempting to open...`,
        );
        try {
          await this.connect();
        } catch (connectError) {
          console.warn(
            `[NodeController ${this.nodeId}] Failed to open connection: ${connectError.message}, continuing anyway...`,
          );
        }
      }

      // Node B needs longer timeout
      const timeout = this.nodeId === "B" ? 8000 : 5000;
      const response = await this.rs485Comm.getStats(timeout);
      if (response.startsWith("OK STATS")) {
        this.stats = this.parseStats(response);
        this.emit("statsUpdated", this.stats);
        return this.stats;
      } else {
        throw new Error(`Get stats failed: ${response}`);
      }
    } catch (error) {
      this.lastError = error.message;
      throw error;
    }
  }

  /**
   * Parse stats from GET_STATS response
   * Format (Node B): OK STATS total_rx=100 lost_pkts=5 crc_err=2 rssi_avg=-65 snr_avg=25 pre_q_avg=128
   * Format (Node A): OK STATS total_sent=100 last_error=0
   * @param {string} response
   * @returns {Object}
   */
  parseStats(response) {
    const stats = {};
    const parts = response.split(" ");

    for (let i = 2; i < parts.length; i++) {
      const [key, value] = parts[i].split("=");
      if (key && value !== undefined) {
        const numValue = parseFloat(value);
        stats[key] = isNaN(numValue) ? value : numValue;
      }
    }

    // Map firmware v2 format to expected format
    // Firmware sends: sent=, attempted=, errors=, timeouts=, last_err=, frame_dur=
    // But parser expects: total_sent, total_attempted, tx_errors, tx_timeouts, last_error, frame_duration_us
    if (stats.sent !== undefined && stats.total_sent === undefined) {
      stats.total_sent = stats.sent;
    }
    if (stats.attempted !== undefined && stats.total_attempted === undefined) {
      stats.total_attempted = stats.attempted;
    }
    if (stats.errors !== undefined && stats.tx_errors === undefined) {
      stats.tx_errors = stats.errors;
    }
    if (stats.timeouts !== undefined && stats.tx_timeouts === undefined) {
      stats.tx_timeouts = stats.timeouts;
    }
    if (stats.last_err !== undefined && stats.last_error === undefined) {
      stats.last_error = stats.last_err;
    }
    if (
      stats.frame_dur !== undefined &&
      stats.frame_duration_us === undefined
    ) {
      stats.frame_duration_us = stats.frame_dur;
    }

    // Node A (TX) stats
    if (this.nodeId === "A") {
      // Firmware v2 sends: sent=, attempted=, errors=, timeouts=, last_err=, frame_dur=
      // Map firmware format to expected format
      const result = {
        total_sent: stats.total_sent || stats.sent || 0,
        total_attempted: stats.total_attempted || stats.attempted || 0,
        tx_errors: stats.tx_errors || stats.errors || 0,
        tx_timeouts: stats.tx_timeouts || stats.timeouts || 0,
        last_error: stats.last_error || stats.last_err || 0,
        frame_duration_us: stats.frame_duration_us || stats.frame_dur || 0,
      };
      return result;
    }

    // Node B (RX) stats
    // Handle case where Node B firmware might be built as Node A type
    // If we get total_sent instead of total_rx, it means wrong firmware type
    if (stats.total_sent !== undefined && stats.total_rx === undefined) {
      console.warn(
        `[NodeController] Node B returned total_sent instead of total_rx - firmware may be built as Node A type!`,
      );
      console.warn(`[NodeController] Response was: ${response}`);
      // Return zeros for RX stats since we can't parse TX stats as RX stats
      return {
        total_rx: 0,
        lost_pkts: 0,
        crc_err: 0,
        rssi_avg_dbm: 0,
        snr_avg_db: 0,
        preamble_q_avg: 0,
      };
    }

    // Normal Node B (RX) stats
    // Firmware v2 sends: rx=, lost=, crc_err=, phy_err=, timeout=, overrun=, rssi_min=, rssi_max=, pre_q_min=, pre_q_max=
    // Map to expected format
    const result = {
      total_rx: stats.total_rx || stats.rx || 0,
      lost_pkts: stats.lost_pkts || stats.lost || 0,
      crc_err: stats.crc_err || 0,
      phy_err: stats.phy_err || 0,
      rx_timeout: stats.rx_timeout || stats.timeout || 0,
      rx_overrun: stats.rx_overrun || stats.overrun || 0,
      rssi_avg_dbm: stats.rssi_avg || 0, // Calculated from rssi_min/rssi_max if needed
      rssi_min: stats.rssi_min || 0,
      rssi_max: stats.rssi_max || 0,
      snr_avg_db: stats.snr_avg || 0,
      preamble_q_avg: stats.pre_q_avg || 0,
      pre_q_min: stats.pre_q_min || 0,
      pre_q_max: stats.pre_q_max || 0,
    };
    return result;
  }

  /**
   * Reset statistics
   * @returns {Promise<boolean>}
   */
  async resetStats() {
    try {
      // Node B needs longer timeout
      const timeout = this.nodeId === "B" ? 8000 : 5000;
      const response = await this.rs485Comm.resetStats(timeout);
      if (response.startsWith("OK")) {
        this.stats = null;
        this.emit("statsReset");
        return true;
      } else {
        throw new Error(`Reset stats failed: ${response}`);
      }
    } catch (error) {
      this.lastError = error.message;
      throw error;
    }
  }

  /**
   * Get current configuration
   * @returns {Object|null}
   */
  getConfig() {
    return this.config;
  }

  /**
   * Get current statistics
   * @returns {Object|null}
   */
  getStatsSync() {
    return this.stats;
  }

  /**
   * Get last error
   * @returns {string|null}
   */
  getLastError() {
    return this.lastError;
  }
}
