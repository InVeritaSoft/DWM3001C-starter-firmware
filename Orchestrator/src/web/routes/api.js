import express from "express";

/**
 * API Routes
 * REST API endpoints for test orchestrator
 */
export function createApiRoutes(nodeA, nodeB, testRunner, csvLogger, config) {
  const router = express.Router();

  /**
   * @swagger
   * /api/status:
   *   get:
   *     summary: Get current test status
   *     tags: [Status]
   *     responses:
   *       200:
   *         description: Current test status
   *         content:
   *           application/json:
   *             schema:
   *               type: object
   *               properties:
   *                 nodeA:
   *                   type: object
   *                   properties:
   *                     state: { type: string }
   *                     connected: { type: boolean }
   *                 nodeB:
   *                   type: object
   *                   properties:
   *                     state: { type: string }
   *                     connected: { type: boolean }
   *                 testRunning: { type: boolean }
   */
  router.get("/status", (req, res) => {
    res.json({
      nodeA: {
        state: nodeA.getState(),
        connected: nodeA.isConnected(),
        lastError: nodeA.getLastError(),
      },
      nodeB: {
        state: nodeB.getState(),
        connected: nodeB.isConnected(),
        lastError: nodeB.getLastError(),
      },
      testRunning: testRunner.isRunning,
    });
  });

  /**
   * @swagger
   * /api/stats:
   *   get:
   *     summary: Get latest stats from both nodes
   *     tags: [Stats]
   *     responses:
   *       200:
   *         description: Latest statistics
   *         content:
   *           application/json:
   *             schema:
   *               type: object
   *               properties:
   *                 nodeA: { type: object }
   *                 nodeB: { type: object }
   */
  router.get("/stats", async (req, res) => {
    try {
      const statsA = nodeA.getStatsSync();
      const statsB = nodeB.getStatsSync();

      res.json({
        nodeA: statsA || null,
        nodeB: statsB || null,
      });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/history:
   *   get:
   *     summary: Get historical CSV data
   *     tags: [History]
   *     parameters:
   *       - in: query
   *         name: page
   *         schema: { type: integer, default: 1 }
   *       - in: query
   *         name: limit
   *         schema: { type: integer, default: 100 }
   *       - in: query
   *         name: file
   *         schema: { type: string }
   *     responses:
   *       200:
   *         description: Historical data
   */
  router.get("/history", async (req, res) => {
    try {
      const page = parseInt(req.query.page) || 1;
      const limit = parseInt(req.query.limit) || 100;
      const file = req.query.file;

      if (file) {
        // Read specific file
        const data = await csvLogger.readLogFile(file);
        const start = (page - 1) * limit;
        const end = start + limit;
        res.json({
          data: data.slice(start, end),
          total: data.length,
          page,
          limit,
        });
      } else {
        // List all log files
        const files = csvLogger.listLogFiles();
        res.json({ files });
      }
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/test-plan:
   *   get:
   *     summary: Get current test plan
   *     tags: [Test Plan]
   *     responses:
   *       200:
   *         description: Test plan
   */
  router.get("/test-plan", (req, res) => {
    try {
      const testPlan = config.getTestPlan();
      res.json({ tests: testPlan });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/test/start:
   *   post:
   *     summary: Start test manually
   *     tags: [Test Control]
   *     requestBody:
   *       content:
   *         application/json:
   *           schema:
   *             type: object
   *             properties:
   *               test: { type: object }
   *     responses:
   *       200:
   *         description: Test started
   */
  router.post("/test/start", async (req, res) => {
    try {
      const { test } = req.body;
      if (!test) {
        return res.status(400).json({ error: "Test configuration required" });
      }

      // Start test asynchronously (don't await - let it run in background)
      // This allows the API to return immediately while the test runs
      testRunner.runTest(test).catch((error) => {
        console.error("Test execution error:", error);
        // Error is already emitted via testRunner events, which Socket.io will broadcast
      });

      // Return immediately - test is running in background
      res.json({ message: "Test started" });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/test/stop:
   *   post:
   *     summary: Stop current test
   *     tags: [Test Control]
   *     responses:
   *       200:
   *         description: Test stopped
   */
  router.post("/test/stop", async (req, res) => {
    try {
      await testRunner.stopTest();
      res.json({ message: "Test stopped" });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/connect:
   *   post:
   *     summary: Connect to both nodes
   *     tags: [Node Control]
   *     responses:
   *       200:
   *         description: Connection results
   */
  router.post("/nodes/connect", async (req, res) => {
    try {
      const results = await Promise.allSettled([
        nodeA.connect(),
        nodeB.connect(),
      ]);

      res.json({
        nodeA:
          results[0].status === "fulfilled"
            ? "connected"
            : `error: ${results[0].reason.message}`,
        nodeB:
          results[1].status === "fulfilled"
            ? "connected"
            : `error: ${results[1].reason.message}`,
      });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/disconnect:
   *   post:
   *     summary: Disconnect from both nodes
   *     tags: [Node Control]
   *     responses:
   *       200:
   *         description: Disconnected
   */
  router.post("/nodes/disconnect", async (req, res) => {
    try {
      await Promise.allSettled([nodeA.disconnect(), nodeB.disconnect()]);
      res.json({ message: "Nodes disconnected" });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/ping:
   *   post:
   *     summary: Ping specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Ping result
   */
  router.post("/nodes/:nodeId/ping", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const result = await node.ping();
      res.json({ success: result });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/send:
   *   post:
   *     summary: Send custom command to specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     requestBody:
   *       required: true
   *       content:
   *         application/json:
   *           schema:
   *             type: object
   *             properties:
   *               command: { type: string }
   *               timeout: { type: number }
   *     responses:
   *       200:
   *         description: Command response
   */
  router.post("/nodes/:nodeId/send", async (req, res) => {
    try {
      const { command, timeout } = req.body;
      if (!command) {
        return res.status(400).json({ error: "Command required" });
      }

      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const response = await node.rs485Comm.sendCommand(command, timeout);
      res.json({ response });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/configure:
   *   post:
   *     summary: Configure specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     requestBody:
   *       required: true
   *       content:
   *         application/json:
   *           schema:
   *             type: object
   *             properties:
   *               channel: { type: number }
   *               data_rate: { type: string }
   *               preamble_len: { type: number }
   *               payload_len: { type: number }
   *               tx_power_idx: { type: number }
   *               pkt_rate_hz: { type: number }
   *     responses:
   *       200:
   *         description: Configuration result
   */
  router.post("/nodes/:nodeId/configure", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const result = await node.configure(req.body);
      res.json({ success: result });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/stats:
   *   get:
   *     summary: Get stats from specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Node statistics
   */
  router.get("/nodes/:nodeId/stats", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const stats = await node.getStats();
      res.json(stats);
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/start:
   *   post:
   *     summary: Start test on specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Node started
   */
  router.post("/nodes/:nodeId/start", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const nodeId = node.nodeId;

      // Ensure node is connected before attempting any operations
      if (!node.isConnected()) {
        console.log(
          `[API] Node ${nodeId} not connected, attempting to connect...`,
        );
        try {
          await node.connect();
          console.log(`[API] Node ${nodeId} connected successfully`);
        } catch (connectError) {
          return res.status(400).json({
            error: `Node ${nodeId} connection failed: ${connectError.message}. Please check serial port configuration.`,
          });
        }
      }

      // Verify node is responding with PING before attempting configuration
      console.log(`[API] Verifying Node ${nodeId} is responding...`);
      try {
        const pingResult = await node.ping();
        if (!pingResult) {
          return res.status(400).json({
            error: `Node ${nodeId} is not responding to PING commands. Please check firmware and hardware connections.`,
            details: {
              nodeState: node.getState(),
              connected: node.isConnected(),
              port: node.rs485Comm.port,
              lastError: node.lastError,
            },
          });
        }
        console.log(`[API] Node ${nodeId} PING successful`);
      } catch (pingError) {
        return res.status(400).json({
          error: `Node ${nodeId} is not responding to PING: ${pingError.message}. Cannot proceed with START.`,
          details: {
            nodeState: node.getState(),
            connected: node.isConnected(),
            port: node.rs485Comm.port,
            lastError: node.lastError,
            suggestion:
              "Check firmware is running, RS-485 hardware connection, and serial port configuration.",
          },
        });
      }

      // Skip auto-configuration - just try to start directly
      // If firmware returns "ERR NOT_CONFIGURED", we'll handle that error
      console.log(
        `[API] Starting test on Node ${nodeId} (skipping auto-configuration)...`,
      );
      try {
        const result = await node.startTest();
        res.json({ success: result });
      } catch (startError) {
        // Handle "ERR NOT_CONFIGURED" from firmware
        if (
          startError.message &&
          startError.message.includes("NOT_CONFIGURED")
        ) {
          return res.status(400).json({
            error: `Node ${nodeId} is not configured. Please configure the node first using the CFG button.`,
            details: {
              nodeState: node.getState(),
              connected: node.isConnected(),
              port: node.rs485Comm.port,
              firmwareError: startError.message,
            },
          });
        }
        // Re-throw other errors to be handled by outer catch
        throw startError;
      }
    } catch (error) {
      console.error(`[API] START command error:`, error.message);
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/stop:
   *   post:
   *     summary: Stop test on specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Node stopped
   */
  router.post("/nodes/:nodeId/stop", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const result = await node.stopTest();
      res.json({ success: result });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/cfg:
   *   post:
   *     summary: Configure specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Configuration command sent
   */
  router.post("/nodes/:nodeId/cfg", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const response = await node.rs485Comm.sendCommand("CFG");
      res.json({ response });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/node-type:
   *   post:
   *     summary: Get node type from specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Node type command sent
   */
  router.post("/nodes/:nodeId/node-type", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const response = await node.rs485Comm.sendCommand("NODE_TYPE");
      res.json({ response });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/config:
   *   get:
   *     summary: Get current configuration of a node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Node configuration
   */
  router.get("/nodes/:nodeId/config", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const config = node.getConfig();
      res.json({
        config: config,
        state: node.getState(),
        connected: node.isConnected(),
      });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/nodes/{nodeId}/reset-stats:
   *   post:
   *     summary: Reset statistics on specific node
   *     tags: [Node Control]
   *     parameters:
   *       - in: path
   *         name: nodeId
   *         required: true
   *         schema: { type: string, enum: [A, B] }
   *     responses:
   *       200:
   *         description: Statistics reset
   */
  router.post("/nodes/:nodeId/reset-stats", async (req, res) => {
    try {
      const node = req.params.nodeId.toUpperCase() === "A" ? nodeA : nodeB;
      const response = await node.rs485Comm.sendCommand("RST");
      res.json({ response });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  /**
   * @swagger
   * /api/test/report:
   *   post:
   *     summary: Generate test report after both nodes stop
   *     tags: [Test Control]
   *     responses:
   *       200:
   *         description: Test report generated
   */
  router.post("/test/report", async (req, res) => {
    try {
      // Get stats from both nodes
      const [statsA, statsB] = await Promise.allSettled([
        nodeA.getStats().catch(() => null),
        nodeB.getStats().catch(() => null),
      ]);

      const report = {
        timestamp: new Date().toISOString(),
        nodeA: {
          stats: statsA.status === "fulfilled" ? statsA.value : null,
          state: nodeA.getState(),
          connected: nodeA.isConnected(),
          lastError: nodeA.getLastError(),
        },
        nodeB: {
          stats: statsB.status === "fulfilled" ? statsB.value : null,
          state: nodeB.getState(),
          connected: nodeB.isConnected(),
          lastError: nodeB.getLastError(),
        },
      };

      res.json(report);
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  return router;
}
