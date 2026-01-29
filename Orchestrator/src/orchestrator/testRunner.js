import { EventEmitter } from "events";
import readline from "readline";
import { NodeController, NodeState } from "./nodeController.js";
import { CSVLogger } from "./csvLogger.js";

/**
 * Test Runner
 * Orchestrates test sequence per main.md section 5.1
 */
export class TestRunner extends EventEmitter {
  constructor(nodeA, nodeB, csvLogger, config) {
    super();
    this.nodeA = nodeA; // TX initiator
    this.nodeB = nodeB; // RX logger
    this.csvLogger = csvLogger;
    this.config = config;
    this.isRunning = false;
    this.currentTest = null;
    this.pollInterval = null;
    // CRITICAL FIX: Reduce polling interval for real-time chart updates
    // Changed from 2 seconds to 500ms for smoother real-time updates
    this.pollIntervalMs =
      (config.getTestConfig().poll_interval_seconds || 0.5) * 1000;
    this.isPolling = false; // Guard to prevent overlapping polls
    this.skipPrompts = false; // Flag to skip interactive prompts (for web API)

    // Create readline interface for operator prompts (only if stdin is available)
    // Skip if stdin is not available or if we're in web mode
    if (process.stdin.isTTY && process.stdin.readable) {
      this.rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
      });
    } else {
      this.rl = null;
      this.skipPrompts = true; // Auto-skip prompts if stdin not available
    }
  }

  /**
   * Run test from test plan
   * @param {Object} test - Test configuration from test plan
   */
  async runTest(test) {
    if (this.isRunning) {
      throw new Error("Test is already running");
    }

    this.currentTest = test;
    this.isRunning = true;
    this.emit("testStarted", test);

    try {
      // Step 1: Prompt operator to set jammer
      await this.promptJammer(test.jammer_label);

      // Step 2: Configure both nodes
      await this.configureNodes(test.config);

      // Step 3: Create CSV log file
      this.csvLogger.createLogFile(test.run_name);

      // Step 4: Start both nodes
      await this.startNodes();

      // Step 5: Start periodic polling
      this.startPolling(test);

      // Step 6: Wait indefinitely until manually stopped
      // Test will continue running until stopTest() is called
      await this.waitUntilStopped();
    } catch (error) {
      this.emit("error", error);
      await this.stopTest();
      throw error;
    }
  }

  /**
   * Prompt operator to set jammer
   * @param {string} jammerLabel - Expected jammer label
   * @returns {Promise<void>}
   */
  async promptJammer(jammerLabel) {
    // Skip prompt if:
    // 1. Prompts are disabled (web API mode)
    // 2. Readline interface is not available (stdin not available)
    // 3. Not in interactive terminal
    if (this.skipPrompts || !this.rl || !process.stdin.isTTY) {
      console.log(
        `Jammer should be set to: ${jammerLabel} (skipping prompt - web API mode)`,
      );
      return Promise.resolve();
    }

    return new Promise((resolve) => {
      this.rl.question(
        `\nSet jammer to: ${jammerLabel} and press ENTER to continue...\n`,
        () => {
          console.log(`Jammer set to: ${jammerLabel}`);
          resolve();
        },
      );
    });
  }

  /**
   * Configure both nodes simultaneously
   * @param {Object} uwbConfig - UWB configuration
   */
  async configureNodes(uwbConfig) {
    this.emit("configuring");

    try {
      console.log("\n" + "=".repeat(60));
      console.log("CONFIGURING BOTH NODES SIMULTANEOUSLY");
      console.log("=".repeat(60));
      console.log(
        `Node A state: ${this.nodeA.getState()}, connected: ${this.nodeA.isConnected()}`,
      );
      console.log(
        `Node B state: ${this.nodeB.getState()}, connected: ${this.nodeB.isConnected()}`,
      );

      const greenColor = "\x1b[32m";
      const resetColor = "\x1b[0m";
      const [resultA, resultB] = await Promise.allSettled([
        this.nodeA
          .configure(uwbConfig)
          .then(() => {
            console.log(
              `${greenColor}✓ Node A configured successfully${resetColor}`,
            );
            return "Node A";
          })
          .catch((err) => {
            console.error(`✗ Node A configuration failed: ${err.message}`);
            throw err;
          }),
        this.nodeB
          .configure(uwbConfig)
          .then(() => {
            console.log(
              `${greenColor}✓ Node B configured successfully${resetColor}`,
            );
            return "Node B";
          })
          .catch((err) => {
            console.error(`✗ Node B configuration failed: ${err.message}`);
            throw err;
          }),
      ]);

      // Check results
      const errors = [];
      if (resultA.status === "rejected") {
        errors.push(`Node A: ${resultA.reason.message || resultA.reason}`);
      }
      if (resultB.status === "rejected") {
        errors.push(`Node B: ${resultB.reason.message || resultB.reason}`);
      }

      if (errors.length > 0) {
        const errorMsg = `Failed to configure nodes: ${errors.join("; ")}`;
        console.error(errorMsg);
        console.log("=".repeat(60) + "\n");
        throw new Error(errorMsg);
      }

      console.log("=".repeat(60) + "\n");
      this.emit("configured", uwbConfig);
    } catch (error) {
      this.emit("error", error);
      throw error;
    }
  }

  /**
   * Start both nodes simultaneously
   */
  async startNodes() {
    this.emit("starting");

    try {
      // Start both nodes simultaneously for synchronized UWB communication
      console.log("Starting both nodes simultaneously...");
      console.log(
        `Node A state: ${this.nodeA.getState()}, connected: ${this.nodeA.isConnected()}`,
      );
      console.log(
        `Node B state: ${this.nodeB.getState()}, connected: ${this.nodeB.isConnected()}`,
      );

      console.log("\n" + "=".repeat(60));
      console.log("SENDING START COMMANDS TO BOTH NODES SIMULTANEOUSLY");
      console.log("=".repeat(60));

      const greenColor = "\x1b[32m";
      const resetColor = "\x1b[0m";
      const [resultA, resultB] = await Promise.allSettled([
        this.nodeA
          .startTest()
          .then(() => {
            console.log(
              `${greenColor}✓ Node A started successfully${resetColor}`,
            );
            return "Node A";
          })
          .catch((err) => {
            console.error(`✗ Node A start failed: ${err.message}`);
            throw err;
          }),
        this.nodeB
          .startTest()
          .then(() => {
            console.log(
              `${greenColor}✓ Node B started successfully${resetColor}`,
            );
            return "Node B";
          })
          .catch((err) => {
            console.error(`✗ Node B start failed: ${err.message}`);
            throw err;
          }),
      ]);

      console.log("=".repeat(60) + "\n");

      // Check results
      const errors = [];
      if (resultA.status === "rejected") {
        errors.push(`Node A: ${resultA.reason.message || resultA.reason}`);
      }
      if (resultB.status === "rejected") {
        errors.push(`Node B: ${resultB.reason.message || resultB.reason}`);
      }

      if (errors.length > 0) {
        const errorMsg = `Failed to start nodes: ${errors.join("; ")}`;
        console.error(errorMsg);
        throw new Error(errorMsg);
      }

      this.emit("started");
    } catch (error) {
      this.emit("error", error);
      throw error;
    }
  }

  /**
   * Start periodic polling
   * @param {Object} test - Test configuration
   */
  startPolling(test) {
    this.emit("pollingStarted");

    this.pollInterval = setInterval(async () => {
      // Prevent overlapping polls - if previous poll is still running, skip this one
      if (this.isPolling) {
        if (process.env.DEBUG_RS485) {
          console.log("[POLL] Previous poll still running, skipping...");
        }
        return;
      }

      this.isPolling = true;
      try {
        await this.pollAndLog(test);
      } catch (error) {
        this.emit("error", error);
      } finally {
        this.isPolling = false;
      }
    }, this.pollIntervalMs);
  }

  /**
   * Poll both nodes and log data
   * @param {Object} test - Test configuration
   */
  async pollAndLog(test) {
    try {
      // Get stats from both nodes
      const statsA = await this.nodeA.getStats();
      const statsB = await this.nodeB.getStats();

      // Get current configuration
      const configA = this.nodeA.getConfig();
      const configB = this.nodeB.getConfig();

      // Log Node A data (TX node - has total_sent, last_error)
      if (statsA) {
        await this.csvLogger.logData({
          timestamp: new Date().toISOString(),
          run_name: test.run_name,
          node_id: "A",
          link_distance_m: test.d_link,
          env_type: test.env_type,
          channel: configA?.channel || test.config.channel,
          data_rate: configA?.data_rate || test.config.data_rate,
          tx_power_idx: configA?.tx_power_idx || test.config.tx_power_idx,
          pkt_rate_hz: configA?.pkt_rate_hz || test.config.pkt_rate_hz,
          jammer_label: test.jammer_label,
          // Node A is TX - map TX stats to CSV schema
          // total_sent represents packets sent (conceptually similar to total_rx)
          total_rx: statsA.total_sent || 0, // Map total_sent to total_rx for CSV
          lost_pkts: 0, // Node A doesn't track lost packets (that's Node B's job)
          crc_err: statsA.last_error || 0, // Map last_error to crc_err for CSV
          rssi_avg_dbm: 0, // Node A doesn't measure RSSI (TX node)
          snr_avg_db: 0, // Node A doesn't measure SNR (TX node)
          preamble_q_avg: 0, // Node A doesn't measure preamble quality (TX node)
        });
      }

      // Log Node B data
      if (statsB) {
        await this.csvLogger.logData({
          timestamp: new Date().toISOString(),
          run_name: test.run_name,
          node_id: "B",
          link_distance_m: test.d_link,
          env_type: test.env_type,
          channel: configB?.channel || test.config.channel,
          data_rate: configB?.data_rate || test.config.data_rate,
          tx_power_idx: configB?.tx_power_idx || test.config.tx_power_idx,
          pkt_rate_hz: configB?.pkt_rate_hz || test.config.pkt_rate_hz,
          jammer_label: test.jammer_label,
          total_rx: statsB.total_rx || 0,
          lost_pkts: statsB.lost_pkts || 0,
          crc_err: statsB.crc_err || 0,
          rssi_avg_dbm: statsB.rssi_avg_dbm || 0,
          snr_avg_db: statsB.snr_avg_db || 0,
          preamble_q_avg: statsB.preamble_q_avg || 0,
        });
      }

      // Emit stats for real-time updates
      this.emit("stats", { nodeA: statsA, nodeB: statsB });
    } catch (error) {
      this.emit("error", error);
    }
  }

  /**
   * Stop test
   */
  async stopTest() {
    if (!this.isRunning) {
      return;
    }

    this.isRunning = false;
    this.emit("stopping");

    // Stop polling
    if (this.pollInterval) {
      clearInterval(this.pollInterval);
      this.pollInterval = null;
    }

    try {
      // Stop both nodes simultaneously
      console.log("Stopping both nodes simultaneously...");
      console.log(
        `Node A state: ${this.nodeA.getState()}, connected: ${this.nodeA.isConnected()}`,
      );
      console.log(
        `Node B state: ${this.nodeB.getState()}, connected: ${this.nodeB.isConnected()}`,
      );

      const greenColor = "\x1b[32m";
      const resetColor = "\x1b[0m";
      const [resultA, resultB] = await Promise.allSettled([
        this.nodeA
          .stopTest()
          .then(() => {
            console.log(
              `${greenColor}✓ Node A stopped successfully${resetColor}`,
            );
            return "Node A";
          })
          .catch((err) => {
            console.error(`✗ Node A stop failed: ${err.message}`);
            throw err;
          }),
        this.nodeB
          .stopTest()
          .then(() => {
            console.log(
              `${greenColor}✓ Node B stopped successfully${resetColor}`,
            );
            return "Node B";
          })
          .catch((err) => {
            console.error(`✗ Node B stop failed: ${err.message}`);
            throw err;
          }),
      ]);

      // Check results
      const errors = [];
      if (resultA.status === "rejected") {
        errors.push(`Node A: ${resultA.reason.message || resultA.reason}`);
      }
      if (resultB.status === "rejected") {
        errors.push(`Node B: ${resultB.reason.message || resultB.reason}`);
      }

      if (errors.length > 0) {
        console.warn(`Some nodes failed to stop: ${errors.join("; ")}`);
        // Don't throw - we still want to emit stopped event
      }

      this.emit("stopped");
      this.emit("testStopped"); // Also emit testStopped for web server compatibility
    } catch (error) {
      this.emit("error", error);
      throw error;
    }
  }

  /**
   * Run test plan (multiple tests)
   * @param {Array<Object>} tests - Array of test configurations
   */
  async runTestPlan(tests) {
    for (let i = 0; i < tests.length; i++) {
      const test = tests[i];
      console.log(
        `\n=== Running test ${i + 1}/${tests.length}: ${test.run_name} ===`,
      );

      try {
        await this.runTest(test);
        const greenColor = "\x1b[32m";
        const resetColor = "\x1b[0m";
        console.log(
          `${greenColor}Test ${test.run_name} completed successfully${resetColor}`,
        );
      } catch (error) {
        console.error(`Test ${test.run_name} failed:`, error.message);
        this.emit("testFailed", { test, error });
      }

      // Wait a bit between tests
      if (i < tests.length - 1) {
        console.log("Waiting 5 seconds before next test...");
        await this.sleep(5000);
      }
    }
  }

  /**
   * Wait until test is stopped (manually)
   * Resolves when isRunning becomes false
   */
  async waitUntilStopped() {
    return new Promise((resolve) => {
      // Check if already stopped
      if (!this.isRunning) {
        resolve();
        return;
      }

      // Poll every 100ms to check if test was stopped
      const checkInterval = setInterval(() => {
        if (!this.isRunning) {
          clearInterval(checkInterval);
          resolve();
        }
      }, 100);
    });
  }

  /**
   * Sleep utility
   * @param {number} ms - Milliseconds to sleep
   */
  sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  /**
   * Close readline interface
   */
  close() {
    if (this.rl) {
      this.rl.close();
      this.rl = null;
    }
  }
}
