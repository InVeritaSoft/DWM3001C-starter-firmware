import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";
import { EventEmitter } from "events";

/**
 * Custom error class for corrupted responses that should be retried
 */
export class CorruptedResponseError extends Error {
  constructor(message, corruptedData, highBitRatio) {
    super(message);
    this.name = "CorruptedResponseError";
    this.corruptedData = corruptedData;
    this.highBitRatio = highBitRatio;
    this.isRetryable = true;
  }
}

/**
 * RS-485 Communication Module
 * Handles serial communication with UWB nodes via RS-485
 */
export class RS485Comm extends EventEmitter {
  constructor(port, baudrate = 57600, timeout = 1000, maxRetries = 3) {
    super();
    this.port = port;
    this.baudrate = baudrate;
    this.timeout = timeout;
    this.maxRetries = maxRetries; // Maximum retries for corrupted responses
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

      // Handle incoming data
      this.parser.on("data", (data) => {
        const timestamp = Date.now();
        const trimmed = data.toString().trim();
        // Always log RX for debugging (not just when DEBUG_RS485 is set)
        console.log(`[RS485 RX] ${this.port}: ${trimmed}`);
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

        // Detect potential corruption patterns
        if (hex.length > 10) {
          const hexBytes = hex.match(/.{2}/g) || [];
          const hexHighBitCount = hexBytes.filter(
            (b) => parseInt(b, 16) >= 0x80
          ).length;
          const repeatingPattern = hex.match(/(.{2})\1{3,}/);
          const alternatingPattern = hex.match(/f[0-9a-f]f[0-9a-f]f[0-9a-f]/i);
          const highBitRatio = hexHighBitCount / hexBytes.length;

          // Lower threshold for corruption detection (10% instead of 50%)
          // This catches corruption earlier, especially for Node B
          if (
            highBitRatio > 0.1 ||
            repeatingPattern ||
            alternatingPattern
          ) {
            console.error(
              `[RS485 ERROR @${timestamp}] ${this.port}: ⚠️ DATA CORRUPTION DETECTED!`
            );
            console.error(
              `[RS485 ERROR @${timestamp}] ${this.port}: Configured: ${this.baudrate} baud, but receiving corrupted data`
            );
            console.error(
              `[RS485 ERROR @${timestamp}] ${this.port}: High-bit ratio: ${(highBitRatio * 100).toFixed(1)}% (should be < 10%)`
            );
            console.error(
              `[RS485 ERROR @${timestamp}] ${this.port}: Corrupted hex: ${hex.substring(0, 40)}${hex.length > 40 ? '...' : ''}`
            );
            console.error(
              `[RS485 ERROR @${timestamp}] ${this.port}: ASCII preview: ${ascii.substring(0, 40)}${ascii.length > 40 ? '...' : ''}`
            );
            
            // Check if it starts with ERR (firmware is responding but data is corrupted)
            if (ascii.toUpperCase().startsWith("ERR")) {
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}: ⚠️ Firmware IS responding (starts with ERR), but data is corrupted during RS485 transmission`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}: This indicates a HARDWARE issue with Node B's RS485 transceiver or wiring`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}: Troubleshooting steps:`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}:   1. Check RS485 transceiver power (VCC→5V, GND→GND) on Node B Arduino`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}:   2. Verify RS485 wiring (A+/B- lines, GND) for Node B`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}:   3. Check termination resistors (120Ω at each end of RS485 bus)`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}:   4. Verify DE/RE pins on Node B's MAX485 are connected to Arduino D2/D3`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}:   5. Swap RS485 adapters between Node A and Node B to test if adapter is faulty`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}:   6. Check if Node B Arduino Serial Monitor shows baud rate mismatch errors`
              );
            } else {
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}: Expected: 57600 baud for RS485 communication`
              );
              console.error(
                `[RS485 ERROR @${timestamp}] ${this.port}: Check: 1) RS485 adapter baud rate setting, 2) Wiring, 3) Termination resistors`
              );
            }
            console.log(
              `[RS485 INFO @${timestamp}] ${this.port}: Run: node scripts/diagnose-baud-hex.js ${this.port} for diagnostics`
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

      // Wait for firmware initialization (firmware needs time to start after port open)
      // J-Link CDC port opening causes board reset - need longer delay
      console.log(`[RS485] Waiting 5 seconds for firmware initialization...`);
      await new Promise((resolve) => setTimeout(resolve, 5000));
      
      // Flush any garbage data in the buffer from the reset
      console.log(`[RS485] Flushing input buffer...`);
      await new Promise((resolve) => {
        this.serialPort.flush(() => {
          // Read and discard any pending data
          const garbage = this.serialPort.read();
          if (garbage) {
            console.log(`[RS485] Discarded ${garbage.length} bytes of startup garbage`);
          }
          resolve();
        });
      });
      
      // Small delay after flush
      await new Promise((resolve) => setTimeout(resolve, 500));
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
   * Send command and wait for response with automatic retry on corruption
   * @param {string} command - ASCII command to send
   * @param {number} timeout - Timeout in milliseconds
   * @param {number} retryCount - Internal retry counter (used for recursion)
   * @returns {Promise<string>} Response string
   */
  async sendCommand(command, timeout = this.timeout, retryCount = 0) {
    // Auto-open port if not already open (allow sending commands without connection check)
    if (!this.isOpen) {
      console.log(`[RS485] Port ${this.port} not open, attempting to open automatically...`);
      try {
        await this.open();
        console.log(`[RS485] Port ${this.port} opened successfully`);
      } catch (openError) {
        console.warn(`[RS485] Failed to auto-open port ${this.port}: ${openError.message}, continuing anyway...`);
        // Continue anyway - let the command attempt to send
      }
    }

    const commandId = ++this.commandId;
    const commandStr = `${command}\r\n`;

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
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
        if (writeError) {
          clearTimeout(timer);
          this.pendingCommands.delete(commandId);
          reject(writeError);
          return;
        }

        // Ensure data is flushed to hardware (critical for RS-485)
        // This must complete before the Promise can resolve, preventing race conditions
        this.serialPort.drain((drainError) => {
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
    }).catch(async (error) => {
      // Handle corruption errors with automatic retry
      if (error instanceof CorruptedResponseError && error.isRetryable && retryCount < this.maxRetries) {
        const newRetryCount = retryCount + 1;
        console.log(
          `[RS485 RETRY] ${this.port}: Command "${command}" failed due to corruption (${(error.highBitRatio * 100).toFixed(1)}% high-bit bytes)`
        );
        console.log(
          `[RS485 RETRY] ${this.port}: Retrying (attempt ${newRetryCount}/${this.maxRetries})...`
        );
        
        // Small delay before retry to allow hardware to stabilize
        await new Promise((resolve) => setTimeout(resolve, 100));
        
        // Retry the command
        try {
          return await this.sendCommand(command, timeout, newRetryCount);
        } catch (retryError) {
          // If retry also fails with corruption and we've reached max retries, give up
          if (retryError instanceof CorruptedResponseError) {
            if (newRetryCount >= this.maxRetries) {
              console.error(
                `[RS485 ERROR] ${this.port}: Command "${command}" failed after ${this.maxRetries} retries due to persistent corruption`
              );
              console.error(
                `[RS485 ERROR] ${this.port}: This indicates a persistent hardware issue with the RS485 connection`
              );
              throw new Error(
                `Command "${command}" failed after ${this.maxRetries} retries due to corrupted responses. ` +
                `Last corruption: ${(retryError.highBitRatio * 100).toFixed(1)}% high-bit bytes. ` +
                `Check RS485 hardware (transceiver, wiring, termination resistors).`
              );
            }
            // Still have retries left, let it propagate to be caught by outer retry logic
          }
          throw retryError;
        }
      }
      
      // Not a corruption error or max retries reached, throw as-is
      throw error;
    });
  }

  /**
   * Handle incoming response
   * @param {string} data - Response data
   */
  handleResponse(data) {
    const timestamp = Date.now();
    const bytes = data ? Array.from(Buffer.from(data)) : [];
    const highBitCount = bytes.filter((b) => b >= 0x80).length;
    const hasCorruption = bytes.length > 0 && highBitCount / bytes.length > 0.1;
    
    // CAPTURE EVERYTHING - Log all incoming data for analysis
    console.log(`[RS485 HANDLE @${timestamp}] ${this.port}: Processing response: "${data}" (len=${data?.length || 0})`);
    
    // Skip empty lines
    if (!data || data.length === 0) {
      console.log(`[RS485 HANDLE @${timestamp}] ${this.port}: Empty data, skipping`);
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
      // Always log filtered messages to see what firmware is sending - CAPTURE EVERYTHING
      console.log(`[RS485 FILTERED @${Date.now()}] ${this.port}: Filtered debug message: "${data}"`);
      console.log(`[RS485 FILTERED @${Date.now()}] ${this.port}: Filter reason: contains debug markers`);
      return;
    }

    // Check for corruption in the response data BEFORE attempting to fix it
    const responseBytes = Array.from(Buffer.from(data));
    const responseHighBitCount = responseBytes.filter((b) => b >= 0x80).length;
    const responseHighBitRatio = responseBytes.length > 0 ? responseHighBitCount / responseBytes.length : 0;
    
    // If corruption is detected, log it but still try to process
    if (responseHighBitRatio > 0.1) {
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: ⚠️ CORRUPTED RESPONSE DETECTED in handleResponse`
      );
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: High-bit ratio: ${(responseHighBitRatio * 100).toFixed(1)}%`
      );
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: Corrupted data: "${data}"`
      );
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: Hex: ${Buffer.from(data).toString("hex")}`
      );
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: This response will likely fail to match any command`
      );
    }
    
    // Handle responses that might have been corrupted but are still recognizable
    // Try to fix common corruption patterns before matching
    let cleanedData = data;
    let wasFixed = false;
    
    // Fix common corruption patterns for ERR responses:
    // - "RS" or "RR" might be corrupted "ERR" (missing first byte or byte corruption)
    // - "UNKNOWN_AMD" or "UNKNOWN_CND" might be "UNKNOWN_CMD" corrupted
    if ((data.startsWith("RS ") || data.startsWith("RR ")) && data.includes("UNKNOWN")) {
      cleanedData = "ERR" + data.substring(2); // Replace "RS" or "RR" with "ERR"
      wasFixed = true;
      // Fix various corrupted "UNKNOWN_CMD" patterns
      cleanedData = cleanedData.replace(/UNKNOWN_[A-Z]{3}/, "UNKNOWN_CMD");
      console.log(`[RS485 FIX] ${this.port}: Fixed corrupted ERR response: "${data}" -> "${cleanedData}"`);
    }
    
    // Fix corruption where "OK" becomes "UOKN", "UOK", "RR UOKN", etc.
    // Pattern: "RR UOKN" or "UOKN" might be corrupted "OK" response
    if (data.includes("UOKN") || data.includes("UOK") || (data.startsWith("RR ") && data.includes("OK"))) {
      // Try to extract the "OK" part
      const okMatch = data.match(/U?OK[N]?/i);
      if (okMatch) {
        cleanedData = "OK" + data.substring(okMatch[0].length).replace(/^[^A-Z]*/, "");
        wasFixed = true;
        console.log(`[RS485 FIX] ${this.port}: Fixed corrupted OK response: "${data}" -> "${cleanedData}"`);
      }
    }
    
    // Fix single character responses that might be corrupted (like "K" which might be part of "OK")
    if (data.length === 1 && data.toUpperCase() === "K") {
      // "K" might be the last character of "OK" that got split
      // Check if we have a pending command and this might be a late response
      if (this.pendingCommands.size > 0) {
        cleanedData = "OK";
        wasFixed = true;
        console.log(`[RS485 FIX] ${this.port}: Fixed single character "K" -> "OK": "${data}" -> "${cleanedData}"`);
      }
    }
    
    // Special handling for RS485 bridge error messages
    // These arrive when the bridge can't get a response from the DWM3001C
    if (cleanedData.includes("ERR_DWM_NO_RESPONSE") || cleanedData.includes("DWM_NO_RESPONSE")) {
      // This is a bridge-level error, not a firmware response
      // Try to match it to a pending command if available
      const sortedCommands = Array.from(this.pendingCommands.entries()).sort(
        ([id1], [id2]) => id1 - id2
      );
      
      if (sortedCommands.length > 0) {
        const [id, pending] = sortedCommands[0];
        clearTimeout(pending.timer);
        this.pendingCommands.delete(id);
        
        console.log(
          `[RS485 ERROR] ${this.port}: Command "${pending.command}" → Bridge error: DWM3001C not responding`
        );
        console.log(
          `[RS485 ERROR] ${this.port}: This indicates the DWM3001C firmware may not be running or there's a baud rate mismatch between the RS485 bridge and DWM3001C`
        );
        console.log(
          `[RS485 ERROR] ${this.port}: Check: 1) DWM3001C power, 2) Bridge→DWM wiring, 3) DWM firmware running, 4) Bridge baud rate (should be 115200 to DWM)`
        );
        
        pending.reject(new Error(`DWM3001C not responding: ${cleanedData}`));
        return;
      } else {
        // No pending command, but log it anyway
        console.log(
          `[RS485 WARNING] ${this.port}: Received ERR_DWM_NO_RESPONSE with no pending command (late response)`
        );
        return;
      }
    }
    
    // Check if response is too corrupted to be useful
    // If high-bit ratio is > 30%, reject it entirely (too corrupted to fix)
    if (responseHighBitRatio > 0.3) {
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: ⚠️ REJECTING HIGHLY CORRUPTED RESPONSE`
      );
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: High-bit ratio: ${(responseHighBitRatio * 100).toFixed(1)}% (threshold: 30%)`
      );
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: Corrupted data: "${data}"`
      );
      console.error(
        `[RS485 ERROR @${timestamp}] ${this.port}: This response is too corrupted to process - will trigger retry`
      );
      
      // Try to match it to a pending command and reject with corruption error for retry
      const sortedCommands = Array.from(this.pendingCommands.entries()).sort(
        ([id1], [id2]) => id1 - id2
      );
      
      if (sortedCommands.length > 0) {
        const [id, pending] = sortedCommands[0];
        clearTimeout(pending.timer);
        this.pendingCommands.delete(id);
        
        const corruptionError = new CorruptedResponseError(
          `Response corrupted (${(responseHighBitRatio * 100).toFixed(1)}% high-bit bytes): ${data}`,
          data,
          responseHighBitRatio
        );
        pending.reject(corruptionError);
        return;
      }
      
      // No pending command, emit as unsolicited data
      this.emit("data", cleanedData);
      return;
    }
    
    // Find matching pending command (FIFO - match oldest command first)
    // This ensures responses match commands in order, preventing race conditions
    if (
      cleanedData.toUpperCase().startsWith("OK") ||
      cleanedData.toUpperCase().startsWith("ERR")
    ) {
      // Get all pending commands sorted by ID (oldest first)
      const sortedCommands = Array.from(this.pendingCommands.entries()).sort(
        ([id1], [id2]) => id1 - id2
      );

      // Match to oldest pending command (FIFO)
      if (sortedCommands.length > 0) {
        const [id, pending] = sortedCommands[0];
        clearTimeout(pending.timer);
        this.pendingCommands.delete(id);

        // Log successful response match
        console.log(
          `[RS485 OK] ${this.port}: Command "${pending.command}" → Response: ${cleanedData}${data !== cleanedData ? ` (fixed from: ${data})` : ""}`
        );

        // Check for corruption in the response (even if it's OK or ERR)
        // If corruption is between 10-30%, retry the command
        if (responseHighBitRatio > 0.1 && responseHighBitRatio <= 0.3) {
          const corruptionError = new CorruptedResponseError(
            `Response corrupted (${(responseHighBitRatio * 100).toFixed(1)}% high-bit bytes): ${cleanedData}`,
            data,
            responseHighBitRatio
          );
          pending.reject(corruptionError);
          return;
        }

        // Response is clean (or corruption < 10%), process normally
        if (cleanedData.toUpperCase().startsWith("OK")) {
          pending.resolve(cleanedData);
        } else {
          pending.reject(new Error(cleanedData));
        }
        return;
      } else {
        // Response arrived but no pending command - might be a late response
        // Store it for a short time in case a command is sent soon
        console.log(
          `[RS485 WARNING] ${this.port}: Received OK/ERR response with no pending command: ${cleanedData}${data !== cleanedData ? ` (fixed from: ${data})` : ""}`
        );
        console.log(
          `[RS485 INFO] ${this.port}: This might be a late response from a previous command that timed out`
        );
      }
    }

    // If no matching command, emit as unsolicited data
    console.log(
      `[RS485 WARNING] ${this.port}: Received data that doesn't match OK/ERR format or has no pending command: ${cleanedData}${data !== cleanedData ? ` (original: ${data})` : ""}`
    );
    this.emit("data", cleanedData);
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
  async getNodeType(timeout = null) {
    return this.sendCommand("NODE_TYPE", timeout || this.timeout);
  }

  /**
   * Send SET_CONFIG command
   * @param {Object} config - Configuration object
   * @param {number} timeout - Optional timeout in milliseconds
   * @returns {Promise<string>}
   */
  async setConfig(config, timeout = null) {
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

    // Use longer timeout for SET_CONFIG commands (firmware waits up to 2 seconds for incomplete commands)
    const configTimeout = timeout || 10000; // Default 10 seconds, but allow override
    const response = await this.sendCommand(command, configTimeout);

    return response;
  }

  /**
   * Send START_TEST command
   * Use short code "STRT" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async startTest(timeout = null) {
    // Firmware supports both "STRT" (short) and "START_TEST" (full)
    console.log(`[RS485] Sending START command to port ${this.port} (isOpen: ${this.isOpen})`);
    return this.sendCommand("STRT", timeout || this.timeout);
  }

  /**
   * Send STOP_TEST command
   * Use short code "STOP" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async stopTest(timeout = null) {
    // Firmware supports both "STOP" (short) and "STOP_TEST" (full)
    // Use short code for better reliability
    return this.sendCommand("STOP", timeout || this.timeout);
  }

  /**
   * Send GET_STATS command
   * Use short code "STAT" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async getStats(timeout = null) {
    // Firmware supports both "STAT" (short) and "GET_STATS" (full)
    return this.sendCommand("STAT", timeout || this.timeout);
  }

  /**
   * Send RESET_STATS command
   * Use short code "RST" for better reliability (firmware supports both)
   * @returns {Promise<string>}
   */
  async resetStats(timeout = null) {
    // Firmware supports both "RST" (short) and "RESET_STATS" (full)
    return this.sendCommand("RST", timeout || this.timeout);
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
