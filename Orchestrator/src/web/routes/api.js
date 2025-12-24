import express from 'express';

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
  router.get('/status', (req, res) => {
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
  router.get('/stats', async (req, res) => {
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
  router.get('/history', async (req, res) => {
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
  router.get('/test-plan', (req, res) => {
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
  router.post('/test/start', async (req, res) => {
    try {
      const { test } = req.body;
      if (!test) {
        return res.status(400).json({ error: 'Test configuration required' });
      }
      
      // Start test asynchronously (don't await - let it run in background)
      // This allows the API to return immediately while the test runs
      testRunner.runTest(test).catch((error) => {
        console.error('Test execution error:', error);
        // Error is already emitted via testRunner events, which Socket.io will broadcast
      });
      
      // Return immediately - test is running in background
      res.json({ message: 'Test started' });
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
  router.post('/test/stop', async (req, res) => {
    try {
      await testRunner.stopTest();
      res.json({ message: 'Test stopped' });
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  return router;
}

