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
          error.message
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
      // Use longer timeout for initial PING (firmware may need time to respond)
      const response = await this.rs485Comm.sendCommand("PNG", 3000);
      return response.startsWith("OK");
    } catch (error) {
      this.lastError = error.message;
      console.error(`⚠️  ${this.nodeId} PING failed - firmware may not be responding`);
      console.error(`   Error: ${error.message.split('\n')[0]}`);
      if (error.message.includes('timeout')) {
        console.error(`   No response received. Check:`);
        console.error(`   1. Firmware is running orchestrator example`);
        console.error(`   2. RS-485 hardware connection`);
        console.error(`   3. Serial port ${this.rs485Comm.port} is correct`);
      }
      return false;
    }
  }

  /**
   * Get firmware node type
   * @returns {Promise<string|null>}
   */
  async getFirmwareNodeType() {
    // #region agent log - getFirmwareNodeType entry
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "nodeController.js:getFirmwareNodeType",
        message: "getFirmwareNodeType called",
        data: { nodeId: this.nodeId, isConnected: this.isConnected() },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "A",
      }),
    }).catch(() => {});
    // #endregion
    try {
      const response = await this.rs485Comm.getNodeType();
      // #region agent log - getFirmwareNodeType response received
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "nodeController.js:getFirmwareNodeType",
            message: "getFirmwareNodeType response received",
            data: {
              nodeId: this.nodeId,
              response,
              responseLength: response?.length,
              startsWithOK: response && response.startsWith("OK NODE_TYPE="),
            },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "A",
          }),
        }
      ).catch(() => {});
      // #endregion
      if (response && response.startsWith("OK NODE_TYPE=")) {
        const type = response.split("=")[1];
        const trimmedType = type.trim();
        // #region agent log - getFirmwareNodeType success
        fetch(
          "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
          {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              location: "nodeController.js:getFirmwareNodeType",
              message: "getFirmwareNodeType success",
              data: { nodeId: this.nodeId, type: trimmedType },
              timestamp: Date.now(),
              sessionId: "debug-session",
              runId: "run1",
              hypothesisId: "A",
            }),
          }
        ).catch(() => {});
        // #endregion
        return trimmedType;
      }
      // #region agent log - getFirmwareNodeType invalid response format
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "nodeController.js:getFirmwareNodeType",
            message: "getFirmwareNodeType invalid response format",
            data: { nodeId: this.nodeId, response },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "A",
          }),
        }
      ).catch(() => {});
      // #endregion
      return null;
    } catch (error) {
      // #region agent log - getFirmwareNodeType error
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "nodeController.js:getFirmwareNodeType",
            message: "getFirmwareNodeType error",
            data: {
              nodeId: this.nodeId,
              errorMessage: error.message,
              errorStack: error.stack,
            },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "A",
          }),
        }
      ).catch(() => {});
      // #endregion
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
      if (this.state === NodeState.RUNNING) {
        throw new Error("Cannot configure while test is running");
      }

      // Verify node is connected and responding before attempting configuration
      if (!this.isConnected()) {
        throw new Error(
          `Node ${this.nodeId} is not connected. Call connect() first.`
        );
      }

      // Quick connectivity check - send PING to verify firmware is responding
      try {
        await this.rs485Comm.ping();
      } catch (pingError) {
        throw new Error(
          `Node ${this.nodeId} is not responding to commands. ` +
            `Port is open but firmware may not be running or RS-485 communication is failing. ` +
            `Original error: ${pingError.message}`
        );
      }

      const response = await this.rs485Comm.setConfig(config);
      // Firmware responds with "OK CONFIG" (not "OK CONFIG_SET")
      if (response.startsWith("OK CONFIG")) {
        this.config = { ...config };
        this.setState(NodeState.CONFIGURED);
        this.emit("configured", config);
        return true;
      } else {
        throw new Error(`Configuration failed: ${response}`);
      }
    } catch (error) {
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
      if (
        this.state !== NodeState.CONFIGURED &&
        this.state !== NodeState.STOPPED
      ) {
        throw new Error(`Cannot start test from state: ${this.state}`);
      }

      const response = await this.rs485Comm.startTest();
      // Firmware responds with "OK START" (not "OK TEST_STARTED")
      if (response.startsWith("OK START")) {
        this.setState(NodeState.RUNNING);
        this.emit("testStarted");
        return true;
      } else {
        throw new Error(`Start test failed: ${response}`);
      }
    } catch (error) {
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
      if (this.state !== NodeState.RUNNING) {
        // Allow stopping from any state
        return true;
      }

      const response = await this.rs485Comm.stopTest();
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
      const response = await this.rs485Comm.getStats();
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
    // #region agent log - parseStats entry
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "nodeController.js:parseStats",
        message: "parseStats called",
        data: { nodeId: this.nodeId, response },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "D",
      }),
    }).catch(() => {});
    // #endregion
    const stats = {};
    const parts = response.split(" ");

    for (let i = 2; i < parts.length; i++) {
      const [key, value] = parts[i].split("=");
      if (key && value !== undefined) {
        const numValue = parseFloat(value);
        stats[key] = isNaN(numValue) ? value : numValue;
      }
    }

    // #region agent log - parseStats parsed keys
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "nodeController.js:parseStats",
        message: "parseStats parsed keys",
        data: {
          nodeId: this.nodeId,
          statsKeys: Object.keys(stats),
          hasTotalSent: stats.total_sent !== undefined,
          hasTotalRx: stats.total_rx !== undefined,
        },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "D",
      }),
    }).catch(() => {});
    // #endregion

    // Node A (TX) stats
    if (this.nodeId === "A") {
      const result = {
        total_sent: stats.total_sent || 0,
        last_error: stats.last_error || 0,
      };
      // #region agent log - parseStats Node A result
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "nodeController.js:parseStats",
            message: "parseStats Node A result",
            data: { nodeId: this.nodeId, result },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "D",
          }),
        }
      ).catch(() => {});
      // #endregion
      return result;
    }

    // Node B (RX) stats
    // Handle case where Node B firmware might be built as Node A type
    // If we get total_sent instead of total_rx, it means wrong firmware type
    if (stats.total_sent !== undefined && stats.total_rx === undefined) {
      // #region agent log - parseStats Node B wrong firmware type
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "nodeController.js:parseStats",
            message: "parseStats Node B wrong firmware type",
            data: { nodeId: this.nodeId, response, stats },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "D",
          }),
        }
      ).catch(() => {});
      // #endregion
      console.warn(
        `[NodeController] Node B returned total_sent instead of total_rx - firmware may be built as Node A type!`
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
    const result = {
      total_rx: stats.total_rx || 0,
      lost_pkts: stats.lost_pkts || 0,
      crc_err: stats.crc_err || 0,
      rssi_avg_dbm: stats.rssi_avg || 0,
      snr_avg_db: stats.snr_avg || 0,
      preamble_q_avg: stats.pre_q_avg || 0,
    };
    // #region agent log - parseStats Node B normal result
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "nodeController.js:parseStats",
        message: "parseStats Node B normal result",
        data: { nodeId: this.nodeId, result },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "D",
      }),
    }).catch(() => {});
    // #endregion
    return result;
  }

  /**
   * Reset statistics
   * @returns {Promise<boolean>}
   */
  async resetStats() {
    try {
      const response = await this.rs485Comm.resetStats();
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
