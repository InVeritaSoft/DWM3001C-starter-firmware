import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";
import { EventEmitter } from "events";

/**
 * RS-485 Communication Module
 * Handles serial communication with UWB nodes via RS-485
 */
export class RS485Comm extends EventEmitter {
  constructor(port, baudrate = 115200, timeout = 1000) {
    super();
    this.port = port;
    this.baudrate = baudrate;
    this.timeout = timeout;
    this.serialPort = null;
    this.parser = null;
    this.isOpen = false;
    this.pendingCommands = new Map();
    this.commandId = 0;
  }

  /**
   * Open serial port connection
   */
  async open() {
    try {
      this.serialPort = new SerialPort({
        path: this.port,
        baudRate: this.baudrate,
        dataBits: 8,
        parity: "none",
        stopBits: 1,
        rtscts: false,
        cts: false,
        dtr: false,
        dsr: false,
        autoOpen: false,
      });

      // Create readline parser
      this.parser = this.serialPort.pipe(
        new ReadlineParser({ delimiter: "\r\n" })
      );

      // Verify parser is set up
      // #region agent log - parser setup
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "rs485Comm.js:open",
            message: "Parser setup complete",
            data: { port: this.port, hasParser: !!this.parser },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "A",
          }),
        }
      ).catch(() => {});
      // #endregion

      // Handle incoming data
      this.parser.on("data", (data) => {
        const timestamp = Date.now();
        const trimmed = data.toString().trim();

        // #region agent log - RS485 data received
        fetch(
          "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
          {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              location: "rs485Comm.js:parser",
              message: "RS485 data received",
              data: {
                raw: data.toString(),
                trimmed,
                hasResponseHandler: !!this.responseHandler,
                pendingCommandsCount: this.pendingCommands.size,
                pendingCommandIds: Array.from(this.pendingCommands.keys()),
                timestamp,
              },
              timestamp: Date.now(),
              sessionId: "debug-session",
              runId: "run1",
              hypothesisId: "B",
            }),
          }
        ).catch(() => {});
        // #endregion

        // CAPTURE EVERYTHING - Enhanced logging with timestamp
        console.log(`[RS485 LINE @${timestamp}] ${this.port}: Complete line received: "${trimmed}"`);
        console.log(
          `[RS485 LINE @${timestamp}] ${this.port}: Hex: ${Buffer.from(data).toString("hex")}`
        );
        console.log(
          `[RS485 LINE @${timestamp}] ${this.port}: Raw bytes: [${Array.from(Buffer.from(data))
            .map((b) => b.toString(16).padStart(2, "0").toUpperCase())
            .join(", ")}]`
        );
        console.log(
          `[RS485 LINE @${timestamp}] ${this.port}: Pending commands: ${
            this.pendingCommands.size
          }, IDs: [${Array.from(this.pendingCommands.keys()).join(", ")}]`
        );
        this.handleResponse(trimmed);
      });

      // Also listen for raw data (in case parser misses something)
      // Note: When using pipe(), the parser consumes data, but raw listener should still fire
      // If firmware sends data, this will catch it even if parser doesn't parse it
      this.serialPort.on("data", (data) => {
        // CAPTURE EVERYTHING - Enhanced logging with timestamp
        const timestamp = Date.now();
        const hex = data.toString("hex");
        const ascii = data.toString("ascii").replace(/[^\x20-\x7E\r\n]/g, ".");
        console.log(`[RS485 RAW @${timestamp}] ${this.port}: ${data.length} bytes - Hex=${hex}, ASCII="${ascii}"`);
        
        // Log individual bytes for detailed analysis
        if (data.length > 0) {
          const byteList = Array.from(data)
            .map((b) => b.toString(16).padStart(2, "0").toUpperCase())
            .join(", ");
          console.log(`[RS485 BYTES @${timestamp}] ${this.port}: Bytes=[${byteList}]`);
        }

        // #region agent log - RS485 raw data received
        fetch(
          "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
          {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              location: "rs485Comm.js:rawData",
              message: "RS485 raw data received",
              data: { port: this.port, hex, ascii, length: data.length, timestamp },
              timestamp: Date.now(),
              sessionId: "debug-session",
              runId: "run1",
              hypothesisId: "B",
            }),
          }
        ).catch(() => {});
        // #endregion

        // Detect potential corruption patterns
        if (hex.length > 10) {
          const bytes = hex.match(/.{2}/g) || [];
          const highBitCount = bytes.filter(
            (b) => parseInt(b, 16) >= 0x80
          ).length;
          const repeatingPattern = hex.match(/(.{2})\1{3,}/);
          const alternatingPattern = hex.match(/f[0-9a-f]f[0-9a-f]f[0-9a-f]/i);

          if (
            highBitCount / bytes.length > 0.5 ||
            repeatingPattern ||
            alternatingPattern
          ) {
            console.log(
              `[RS485 WARNING @${timestamp}] ${this.port}: Potential baud rate mismatch detected!`
            );
            console.log(
              `[RS485 WARNING @${timestamp}] ${this.port}: Run: node scripts/diagnose-baud-hex.js ${this.port}`
            );
          }
        }
      });

      // Wait for port to be ready
      await new Promise((resolve, reject) => {
        const cleanup = () => {
          this.serialPort.removeListener("open", onOpen);
          this.serialPort.removeListener("error", onError);
        };

        const onOpen = () => {
          cleanup();
          this.isOpen = true;
          // #region agent log - serial port opened
          fetch(
            "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
            {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({
                location: "rs485Comm.js:open",
                message: "Serial port opened",
                data: {
                  port: this.port,
                  baudrate: this.baudrate,
                  isOpen: this.isOpen,
                  hasParser: !!this.parser,
                  hasRawListener: this.serialPort.listenerCount("data") > 0,
                },
                timestamp: Date.now(),
                sessionId: "debug-session",
                runId: "run1",
                hypothesisId: "A",
              }),
            }
          ).catch(() => {});
          // #endregion
          console.log(
            `[RS485] Port ${this.port} opened at ${this.baudrate} baud - ready to receive data`
          );
          console.log(
            `[RS485] Raw data listener registered: ${
              this.serialPort.listenerCount("data") > 0
            }, Parser listener registered: ${
              this.parser.listenerCount("data") > 0
            }`
          );
          this.emit("open");
          resolve();
        };

        const onError = (error) => {
          cleanup();
          // Emit error event for listeners, but also reject the promise
          this.emit("error", error);
          reject(error);
        };

        this.serialPort.once("open", onOpen);
        this.serialPort.once("error", onError);

        // Handle errors after port is opened (for ongoing errors)
        this.serialPort.on("error", (error) => {
          // Only emit if port is already open (don't double-handle open errors)
          if (this.isOpen) {
            this.emit("error", error);
          }
        });

        this.serialPort.open();
      });

      // Wait for firmware initialization (firmware needs time to start)
      await new Promise((resolve) => setTimeout(resolve, 2000));
    } catch (error) {
      this.emit("error", error);
      throw error;
    }
  }

  /**
   * Close serial port connection
   */
  async close() {
    if (this.serialPort && this.isOpen) {
      await this.serialPort.close();
      this.isOpen = false;
      this.emit("close");
    }
  }

  /**
   * Send command and wait for response
   * @param {string} command - ASCII command to send
   * @param {number} timeout - Timeout in milliseconds
   * @returns {Promise<string>} Response string
   */
  async sendCommand(command, timeout = this.timeout) {
    // #region agent log - sendCommand entry
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "rs485Comm.js:sendCommand",
        message: "sendCommand called",
        data: {
          command,
          commandLength: command.length,
          timeout,
          isOpen: this.isOpen,
        },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "C",
      }),
    }).catch(() => {});
    // #endregion
    if (!this.isOpen) {
      // #region agent log - sendCommand port not open
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "rs485Comm.js:sendCommand",
            message: "sendCommand port not open",
            data: { isOpen: this.isOpen },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "C",
          }),
        }
      ).catch(() => {});
      // #endregion
      throw new Error(`Port ${this.port} is not open`);
    }

    const commandId = ++this.commandId;
    const commandStr = `${command}\r\n`;

    // #region agent log - sendCommand before write
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "rs485Comm.js:sendCommand",
        message: "sendCommand before write",
        data: { command, commandStr, commandId },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "C",
      }),
    }).catch(() => {});
    // #endregion

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        // #region agent log - sendCommand timeout
        fetch(
          "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
          {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              location: "rs485Comm.js:sendCommand",
              message: "sendCommand timeout",
              data: {
                command,
                commandId,
                timeout,
                pendingCommandsCount: this.pendingCommands.size,
                allPendingCommands: Array.from(
                  this.pendingCommands.entries()
                ).map(([id, cmd]) => ({ id, command: cmd.command })),
              },
              timestamp: Date.now(),
              sessionId: "debug-session",
              runId: "run1",
              hypothesisId: "C",
            }),
          }
        ).catch(() => {});
        // #endregion
        this.pendingCommands.delete(commandId);
        
        // Provide helpful error message with troubleshooting steps
        const errorMsg = `Command timeout: ${command}\n` +
          `  Port: ${this.port}\n` +
          `  Timeout: ${timeout}ms\n` +
          `  No response received from firmware.\n` +
          `\nTroubleshooting:\n` +
          `  1. Check LED behavior on board:\n` +
          `     - Orange LED should blink when command is received\n` +
          `     - Green LED should blink when response is sent\n` +
          `     - If no LEDs blink, firmware may not be receiving commands\n` +
          `  2. Verify firmware is running orchestrator v2:\n` +
          `     - Check LED behavior on startup (should blink)\n` +
          `     - Rebuild and flash: .\\build-and-flash-tx.ps1 or .\\build-and-flash-rx.ps1\n` +
          `  3. Check RS-485 hardware connection:\n` +
          `     - Verify RS-485 transceiver is connected\n` +
          `     - Check wiring (A+/B- lines, GND)\n` +
          `     - Ensure proper termination resistors (120Ω at each end)\n` +
          `     - Verify transceiver enable/DE pins are configured\n` +
          `  4. Verify serial port:\n` +
          `     - Port ${this.port} is correct\n` +
          `     - Baud rate matches firmware (115200)\n` +
          `     - No other software using the port\n` +
          `  5. Test with diagnostic script:\n` +
          `     - Run: npm run test-serial -- ${this.port}\n` +
          `     - This will test PING and NODE_TYPE commands\n` +
          `     - Watch LEDs and check for responses\n` +
          `  6. Test with direct serial terminal:\n` +
          `     - Open serial terminal (PuTTY, Tera Term, etc.)\n` +
          `     - Configure: 115200 baud, 8N1, no flow control\n` +
          `     - Send "PNG\\r\\n" and check for "OK\\r\\n" response\n` +
          `     - Watch LEDs: orange on TX, green on RX`;
        
        reject(new Error(errorMsg));
      }, timeout);

      this.pendingCommands.set(commandId, { resolve, reject, timer, command });

      // Write command to serial port and ensure drain completes before continuing
      // Always log TX for debugging (not just when DEBUG_RS485 is set)
      console.log(
        `[RS485 TX] ${this.port}: ${command.trim()} (${
          commandStr.length
        } bytes including \\r\\n)`
      );
      console.log(
        `[RS485 TX] ${this.port}: Hex: ${Buffer.from(commandStr).toString(
          "hex"
        )}`
      );

      // Write command
      this.serialPort.write(commandStr, (writeError) => {
        // #region agent log - sendCommand write callback
        fetch(
          "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
          {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              location: "rs485Comm.js:sendCommand",
              message: "sendCommand write callback",
              data: { commandId, writeError: writeError?.message },
              timestamp: Date.now(),
              sessionId: "debug-session",
              runId: "run1",
              hypothesisId: "C",
            }),
          }
        ).catch(() => {});
        // #endregion
        if (writeError) {
          clearTimeout(timer);
          this.pendingCommands.delete(commandId);
          reject(writeError);
          return;
        }

        // Ensure data is flushed to hardware (critical for RS-485)
        // This must complete before the Promise can resolve, preventing race conditions
        this.serialPort.drain((drainError) => {
          // #region agent log - sendCommand drain callback
          fetch(
            "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
            {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({
                location: "rs485Comm.js:sendCommand",
                message: "sendCommand drain callback",
                data: { commandId, drainError: drainError?.message },
                timestamp: Date.now(),
                sessionId: "debug-session",
                runId: "run1",
                hypothesisId: "C",
              }),
            }
          ).catch(() => {});
          // #endregion
          if (drainError) {
            clearTimeout(timer);
            this.pendingCommands.delete(commandId);
            reject(drainError);
            return;
          }

          if (process.env.DEBUG_RS485) {
            console.log(
              `[RS485 TX] ${this.port}: Data drained (sent to hardware)`
            );
          }

          // Drain complete - data is now guaranteed to be flushed
          // The outer Promise will resolve when response is received via handleResponse()
        });
      });
    });
  }

  /**
   * Handle incoming response
   * @param {string} data - Response data
   */
  handleResponse(data) {
    const timestamp = Date.now();
    
    // CAPTURE EVERYTHING - Log all incoming data for analysis
    console.log(`[RS485 HANDLE @${timestamp}] ${this.port}: Processing response: "${data}" (len=${data?.length || 0})`);
    
    // #region agent log - handleResponse entry
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "rs485Comm.js:handleResponse",
        message: "handleResponse called",
        data: {
          data,
          dataLength: data?.length,
          pendingCommandsCount: this.pendingCommands.size,
          pendingCommandIds: Array.from(this.pendingCommands.keys()),
          pendingCommands: Array.from(this.pendingCommands.entries()).map(
            ([id, cmd]) => ({ id, command: cmd.command })
          ),
          timestamp,
        },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "B",
      }),
    }).catch(() => {});
    // #endregion
    // Skip empty lines
    if (!data || data.length === 0) {
      console.log(`[RS485 HANDLE @${timestamp}] ${this.port}: Empty data, skipping`);
      // #region agent log - handleResponse empty data
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "rs485Comm.js:handleResponse",
            message: "handleResponse empty data skipped",
            data: { data, dataLength: data?.length },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "B",
          }),
        }
      ).catch(() => {});
      // #endregion
      return;
    }

    // Filter out firmware debug messages
    // Firmware sends: [UART], [CMD], [HANDLER], [PARSER], [UART_TX], [RX], [DBG], etc.
    const lower = data.toLowerCase();
    const isFiltered =
      lower.includes("[uart]") ||
      lower.includes("[cmd]") ||
      lower.includes("[handler]") ||
      lower.includes("[parser]") ||
      lower.includes("[uart_tx]") ||
      lower.includes("[dbg]") ||
      lower.includes("<dbg>") ||
      lower.includes("[rx]") ||  // Filter [RX] byte=0xXX messages
      lower.includes("mpu:") ||
      lower.includes("os:") ||
      (lower.startsWith("[") &&
        !lower.startsWith("[ok") &&
        !lower.startsWith("[err"));

    if (isFiltered) {
      // #region agent log - handleResponse filtered debug message
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "rs485Comm.js:handleResponse",
            message: "handleResponse filtered debug message",
            data: { data, lower, isFiltered },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "B",
          }),
        }
      ).catch(() => {});
      // #endregion
      // Always log filtered messages to see what firmware is sending - CAPTURE EVERYTHING
      console.log(`[RS485 FILTERED @${Date.now()}] ${this.port}: Filtered debug message: "${data}"`);
      console.log(`[RS485 FILTERED @${Date.now()}] ${this.port}: Filter reason: contains debug markers`);
      return;
    }

    // Find matching pending command (FIFO - match oldest command first)
    // This ensures responses match commands in order, preventing race conditions
    if (
      data.toUpperCase().startsWith("OK") ||
      data.toUpperCase().startsWith("ERR")
    ) {
      // #region agent log - handleResponse matched OK/ERR
      fetch(
        "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
        {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            location: "rs485Comm.js:handleResponse",
            message: "handleResponse matched OK/ERR",
            data: {
              data,
              isOK: data.toUpperCase().startsWith("OK"),
              pendingCommandsCount: this.pendingCommands.size,
              pendingCommandIds: Array.from(this.pendingCommands.keys()),
              pendingCommands: Array.from(this.pendingCommands.entries()).map(
                ([id, cmd]) => ({ id, command: cmd.command })
              ),
            },
            timestamp: Date.now(),
            sessionId: "debug-session",
            runId: "run1",
            hypothesisId: "B",
          }),
        }
      ).catch(() => {});
      // #endregion
      // Get all pending commands sorted by ID (oldest first)
      const sortedCommands = Array.from(this.pendingCommands.entries()).sort(
        ([id1], [id2]) => id1 - id2
      );

      // Match to oldest pending command (FIFO)
      if (sortedCommands.length > 0) {
        const [id, pending] = sortedCommands[0];
        // #region agent log - handleResponse resolving command
        fetch(
          "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
          {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              location: "rs485Comm.js:handleResponse",
              message: "handleResponse resolving command",
              data: {
                commandId: id,
                pendingCommand: pending.command,
                response: data,
              },
              timestamp: Date.now(),
              sessionId: "debug-session",
              runId: "run1",
              hypothesisId: "B",
            }),
          }
        ).catch(() => {});
        // #endregion
        clearTimeout(pending.timer);
        this.pendingCommands.delete(id);

        // Log successful response match
        console.log(
          `[RS485 OK] ${this.port}: Command "${pending.command}" → Response: ${data}`
        );

        if (data.toUpperCase().startsWith("OK")) {
          pending.resolve(data);
        } else {
          pending.reject(new Error(data));
        }
        return;
      } else {
        // #region agent log - handleResponse no pending command
        fetch(
          "http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f",
          {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              location: "rs485Comm.js:handleResponse",
              message: "handleResponse no pending command",
              data: { data },
              timestamp: Date.now(),
              sessionId: "debug-session",
              runId: "run1",
              hypothesisId: "B",
            }),
          }
        ).catch(() => {});
        // #endregion
        console.log(
          `[RS485 WARNING] ${this.port}: Received OK/ERR response with no pending command: ${data}`
        );
      }
    }

    // If no matching command, emit as unsolicited data
    // #region agent log - handleResponse unsolicited data
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "rs485Comm.js:handleResponse",
        message: "handleResponse unsolicited data",
        data: {
          data,
          startsWithOK: data.toUpperCase().startsWith("OK"),
          startsWithERR: data.toUpperCase().startsWith("ERR"),
          pendingCommandsCount: this.pendingCommands.size,
        },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "B",
      }),
    }).catch(() => {});
    // #endregion
    console.log(
      `[RS485 WARNING] ${this.port}: Received data that doesn't match OK/ERR format or has no pending command: ${data}`
    );
    this.emit("data", data);
  }

  /**
   * Send PING command
   * Use short code "PNG" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async ping() {
    // Firmware supports both "PNG" (short) and "PING" (full)
    // Use short code for better reliability
    return this.sendCommand("PNG");
  }

  /**
   * Get node type from firmware
   * @returns {Promise<string>}
   */
  async getNodeType() {
    return this.sendCommand("NODE_TYPE");
  }

  /**
   * Send SET_CONFIG command
   * @param {Object} config - Configuration object
   * @returns {Promise<string>}
   */
  async setConfig(config) {
    // Format: SET_CONFIG ch=5 rate=6m8 pl=128 len=64 pwr=5 rate_hz=100
    // Use short code "CFG" for better reliability with long commands
    // Firmware supports both "CFG" (short) and "SET_CONFIG" (full)
    console.log(`[RS485] setConfig() called for port ${this.port} (isOpen: ${this.isOpen})`);
    const params = [];
    if (config.channel !== undefined) params.push(`ch=${config.channel}`);
    if (config.data_rate !== undefined) {
      // Firmware expects "6m8" for 6.8Mbps or "850k" for 850kbps
      // Handle both string and numeric data_rate values
      let rateStr;
      if (typeof config.data_rate === "string") {
        // Already in correct format (e.g., "6m8" or "850k")
        rateStr = config.data_rate;
      } else if (typeof config.data_rate === "number") {
        // Legacy numeric format: 1 = 6.8Mbps, 0 = 850kbps
        rateStr = config.data_rate === 1 ? "6m8" : "850k";
      } else {
        // Default to 6.8Mbps if unknown format
        rateStr = "6m8";
      }
      params.push(`rate=${rateStr}`);
    }
    if (config.preamble_len !== undefined)
      params.push(`pl=${config.preamble_len}`);
    if (config.payload_len !== undefined)
      params.push(`len=${config.payload_len}`);
    if (config.tx_power_idx !== undefined)
      params.push(`pwr=${config.tx_power_idx}`);
    if (config.pkt_rate_hz !== undefined)
      params.push(`rate_hz=${config.pkt_rate_hz}`);

    // Use short code "CFG" instead of "SET_CONFIG" for better reliability
    const command = `CFG ${params.join(" ")}`;
    console.log(`[RS485] Sending CFG command to port ${this.port}: ${command}`);

    // #region agent log - SET_CONFIG entry
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "rs485Comm.js:setConfig",
        message: "SET_CONFIG called",
        data: { config, params, command },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "A",
      }),
    }).catch(() => {});
    // #endregion

    // #region agent log - Debug SET_CONFIG command
    console.log(`[RS485 SET_CONFIG] ${this.port}: Command="${command}"`);
    console.log(
      `[RS485 SET_CONFIG] ${this.port}: Config=`,
      JSON.stringify(config, null, 2)
    );
    console.log(`[RS485 SET_CONFIG] ${this.port}: Params=`, params);
    // #endregion

    // Use longer timeout for SET_CONFIG commands (firmware waits up to 2 seconds for incomplete commands)
    const response = await this.sendCommand(command, 10000); // Increased timeout to 10 seconds

    // #region agent log - SET_CONFIG response
    fetch("http://127.0.0.1:7246/ingest/53b9dbf8-c6bb-42df-aadd-00e84572bd7f", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        location: "rs485Comm.js:setConfig",
        message: "SET_CONFIG response",
        data: { response, isError: response.startsWith("ERR") },
        timestamp: Date.now(),
        sessionId: "debug-session",
        runId: "run1",
        hypothesisId: "A",
      }),
    }).catch(() => {});
    // #endregion

    return response;
  }

  /**
   * Send START_TEST command
   * Use short code "STRT" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async startTest() {
    // Firmware supports both "STRT" (short) and "START_TEST" (full)
    console.log(`[RS485] Sending START command to port ${this.port} (isOpen: ${this.isOpen})`);
    return this.sendCommand("STRT");
  }

  /**
   * Send STOP_TEST command
   * Use short code "STOP" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async stopTest() {
    // Firmware supports both "STOP" (short) and "STOP_TEST" (full)
    // Use short code for better reliability
    return this.sendCommand("STOP");
  }

  /**
   * Send GET_STATS command
   * Use short code "STAT" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async getStats() {
    // Firmware supports both "STAT" (short) and "GET_STATS" (full)
    return this.sendCommand("STAT");
  }

  /**
   * Send RESET_STATS command
   * Use short code "RST" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async resetStats() {
    // Firmware supports both "RST" (short) and "RESET_STATS" (full)
    return this.sendCommand("RST");
  }

  /**
   * Send SET_LOG_MODE command (optional)
   * @param {number} level - Log level (0, 1, or 2)
   * @returns {Promise<string>}
   */
  async setLogMode(level) {
    return this.sendCommand(`SET_LOG_MODE level=${level}`);
  }
}
