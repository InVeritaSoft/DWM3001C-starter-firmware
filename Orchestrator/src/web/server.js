import express from "express";
import { createServer } from "http";
import { Server } from "socket.io";
import cors from "cors";
import path from "path";
import { fileURLToPath } from "url";
import { getConfig } from "../orchestrator/config.js";
import { RS485Comm } from "../orchestrator/rs485Comm.js";
import { NodeController } from "../orchestrator/nodeController.js";
import { CSVLogger } from "../orchestrator/csvLogger.js";
import { TestRunner } from "../orchestrator/testRunner.js";
import { createApiRoutes } from "./routes/api.js";
import { setupSwagger } from "./routes/swagger.js";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

/**
 * Web Server
 * Express.js server with Socket.io for real-time updates
 */
class WebServer {
  constructor() {
    this.app = express();
    this.server = createServer(this.app);
    this.io = new Server(this.server, {
      cors: {
        origin: "*",
        methods: ["GET", "POST"],
      },
    });
    this.config = getConfig();
    this.config.loadSettings();
    this.config.loadTestPlan();

    // Initialize components
    this.csvLogger = new CSVLogger();
    this.nodeA = null;
    this.nodeB = null;
    this.testRunner = null;

    this.setupMiddleware();
    this.setupRoutes();
    this.setupSocketIO();
  }

  /**
   * Setup middleware
   */
  setupMiddleware() {
    this.app.use(cors());
    this.app.use(express.json());
    this.app.use(express.urlencoded({ extended: true }));

    // Serve static files from React build
    const frontendBuild = path.join(__dirname, "../../frontend/dist");
    this.app.use(express.static(frontendBuild));

    // Serve CSV files
    this.app.use(
      "/data/logs",
      express.static(path.join(__dirname, "../../data/logs"))
    );
  }

  /**
   * Setup routes
   */
  setupRoutes() {
    // Initialize nodes and test runner
    const serialConfig = this.config.getSerialConfig();
    const rs485A = new RS485Comm(
      serialConfig.node_a_port,
      serialConfig.baudrate,
      serialConfig.timeout * 1000
    );
    const rs485B = new RS485Comm(
      serialConfig.node_b_port,
      serialConfig.baudrate,
      serialConfig.timeout * 1000
    );

    this.nodeA = new NodeController("A", rs485A);
    this.nodeB = new NodeController("B", rs485B);
    this.testRunner = new TestRunner(
      this.nodeA,
      this.nodeB,
      this.csvLogger,
      this.config
    );

    // Disable interactive prompts when running from web server
    // This prevents the jammer prompt from blocking API requests
    this.testRunner.skipPrompts = true;

    // Setup event listeners for Socket.io
    this.setupEventListeners();

    // API routes
    this.app.use(
      "/api",
      createApiRoutes(
        this.nodeA,
        this.nodeB,
        this.testRunner,
        this.csvLogger,
        this.config
      )
    );

    // Swagger
    setupSwagger(this.app, this.config);

    // Fallback to React app
    this.app.get("*", (req, res) => {
      res.sendFile(path.join(__dirname, "../../frontend/dist/index.html"));
    });
  }

  /**
   * Setup Socket.io
   */
  setupSocketIO() {
    this.io.on("connection", (socket) => {
      console.log("Client connected:", socket.id);

      // Send initial status
      socket.emit("status", {
        nodeA: {
          state: this.nodeA.getState(),
          connected: this.nodeA.isConnected(),
        },
        nodeB: {
          state: this.nodeB.getState(),
          connected: this.nodeB.isConnected(),
        },
        testRunning: this.testRunner.isRunning,
      });

      socket.on("disconnect", () => {
        console.log("Client disconnected:", socket.id);
      });
    });
  }

  /**
   * Setup event listeners for real-time updates
   */
  setupEventListeners() {
    // Node A events
    this.nodeA.on("stateChange", (data) => {
      this.io.emit("nodeStateChange", { node: "A", ...data });
    });

    this.nodeA.on("statsUpdated", (stats) => {
      this.io.emit("stats", { node: "A", stats });
    });

    // Node B events
    this.nodeB.on("stateChange", (data) => {
      this.io.emit("nodeStateChange", { node: "B", ...data });
    });

    this.nodeB.on("statsUpdated", (stats) => {
      this.io.emit("stats", { node: "B", stats });
    });

    // Test runner events
    this.testRunner.on("stats", (data) => {
      this.io.emit("stats", data);
    });

    this.testRunner.on("testStarted", (test) => {
      this.io.emit("testStarted", test);
    });

    this.testRunner.on("testStopped", () => {
      this.io.emit("testStopped");
    });

    this.testRunner.on("error", (error) => {
      this.io.emit("error", { message: error.message });
    });
  }

  /**
   * Start server
   */
  async start() {
    const webConfig = this.config.getWebConfig();
    // IMPORTANT: Backend API must run on port 5000
    // Frontend dev server runs on port 3000 and proxies /api requests to port 5000
    // This separation allows monitoring (frontend on 3000) and commands (API on 5000) to work together
    // Always use 5000 for backend API, regardless of PORT env var (which might be 3000 for frontend)
    const port = 5000; // Force port 5000 for backend API
    const host = webConfig.host || "0.0.0.0";
    
    console.log(`[Server] Backend API server will run on port ${port}`);
    console.log(`[Server] Frontend dev server should run on port 3000 and proxy /api to port ${port}`);

    // Connect to nodes
    try {
      console.log("Connecting to Node A...");
      try {
        await this.nodeA.connect();
        console.log("Node A connected");

        // Send PING first to verify basic communication
        console.log("Pinging Node A...");
        try {
          const pingResult = await this.nodeA.ping();
          if (pingResult) {
            const greenColor = '\x1b[32m';
            const resetColor = '\x1b[0m';
            console.log(`${greenColor}✓ Node A PING successful${resetColor}`);
          } else {
            console.warn(
              "⚠️  Node A PING failed - firmware may not be responding"
            );
          }
        } catch (pingError) {
          console.warn(`⚠️  Node A PING error: ${pingError.message}`);
        }
      } catch (nodeAError) {
        console.error(`Failed to connect to Node A: ${nodeAError.message}`);
        console.log("Node A will not be available, but server will continue");
      }

      console.log("Connecting to Node B...");
      try {
        await this.nodeB.connect();
        console.log("Node B connected");

        // Send PING first to verify basic communication
        console.log("Pinging Node B...");
        try {
          const pingResult = await this.nodeB.ping();
          if (pingResult) {
            const greenColor = '\x1b[32m';
            const resetColor = '\x1b[0m';
            console.log(`${greenColor}✓ Node B PING successful${resetColor}`);
          } else {
            console.warn(
              "⚠️  Node B PING failed - firmware may not be responding"
            );
          }
        } catch (pingError) {
          console.warn(`⚠️  Node B PING error: ${pingError.message}`);
        }
      } catch (nodeBError) {
        console.error(`Failed to connect to Node B: ${nodeBError.message}`);
        console.log("Node B will not be available, but server will continue");
      }
    } catch (error) {
      console.error("Failed to connect to nodes:", error.message);
      console.log("Server will start but nodes may not be available");
    }

    this.server.listen(port, host, () => {
      console.log(`Web server running on http://${host}:${port}`);
      console.log(`Swagger UI available at http://${host}:${port}/api-docs`);
    });
  }

  /**
   * Stop server
   */
  async stop() {
    if (this.testRunner) {
      await this.testRunner.stopTest();
      this.testRunner.close();
    }

    if (this.nodeA) {
      await this.nodeA.disconnect();
    }

    if (this.nodeB) {
      await this.nodeB.disconnect();
    }

    this.server.close();
  }
}

// Start server if run directly
const isMainModule =
  import.meta.url === `file://${process.argv[1]}` ||
  import.meta.url.endsWith(process.argv[1].replace(/\\/g, "/"));

if (isMainModule) {
  const server = new WebServer();
  server.start();

  // Graceful shutdown
  process.on("SIGTERM", async () => {
    console.log("SIGTERM received, shutting down gracefully...");
    await server.stop();
    process.exit(0);
  });

  process.on("SIGINT", async () => {
    console.log("SIGINT received, shutting down gracefully...");
    await server.stop();
    process.exit(0);
  });
}

export default WebServer;
