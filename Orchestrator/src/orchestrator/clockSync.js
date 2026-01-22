import { EventEmitter } from "events";

/**
 * Clock Synchronization Module
 * Provides synchronized timing for all components (Orchestrator, RS485_Bridge, Firmware)
 * Works like a watch - all components tick in sync
 */
export class ClockSync extends EventEmitter {
  constructor(intervalMs = 1000) {
    super();
    this.intervalMs = intervalMs; // Base clock interval in milliseconds
    this.isRunning = false;
    this.tickInterval = null;
    this.tickCount = 0;
    this.startTime = null;
    this.lastTickTime = null;
    this.phaseOffset = 0; // Phase offset for alignment (0-1, where 0 = start of interval)
    
    // Clock state
    this.clockState = {
      tick: 0,
      timestamp: null,
      elapsed: 0,
      phase: 0, // Current phase in interval (0-1)
    };
  }

  /**
   * Start the clock
   * @param {number} phaseOffset - Phase offset (0-1) for alignment
   */
  start(phaseOffset = 0) {
    if (this.isRunning) {
      return;
    }

    this.phaseOffset = phaseOffset;
    this.isRunning = true;
    this.tickCount = 0;
    this.startTime = Date.now();
    this.lastTickTime = this.startTime;

    // Calculate first tick time with phase offset
    const firstTickDelay = this.phaseOffset * this.intervalMs;
    
    // Emit start event
    this.emit("start", {
      intervalMs: this.intervalMs,
      phaseOffset: this.phaseOffset,
      startTime: this.startTime,
    });

    // Schedule first tick
    setTimeout(() => {
      this.tick();
      // Then continue with regular interval
      this.tickInterval = setInterval(() => {
        this.tick();
      }, this.intervalMs);
    }, firstTickDelay);
  }

  /**
   * Stop the clock
   */
  stop() {
    if (!this.isRunning) {
      return;
    }

    this.isRunning = false;
    
    if (this.tickInterval) {
      clearInterval(this.tickInterval);
      this.tickInterval = null;
    }

    this.emit("stop", {
      tickCount: this.tickCount,
      elapsed: Date.now() - this.startTime,
    });
  }

  /**
   * Internal tick handler
   */
  tick() {
    const now = Date.now();
    const elapsed = now - this.startTime;
    const phase = ((now - this.lastTickTime) / this.intervalMs) % 1;
    
    this.tickCount++;
    this.lastTickTime = now;

    // Update clock state
    this.clockState = {
      tick: this.tickCount,
      timestamp: now,
      elapsed: elapsed,
      phase: phase,
    };

    // Emit tick event
    this.emit("tick", {
      ...this.clockState,
      intervalMs: this.intervalMs,
    });
  }

  /**
   * Get current clock state
   * @returns {Object}
   */
  getState() {
    if (!this.isRunning) {
      return {
        tick: 0,
        timestamp: null,
        elapsed: 0,
        phase: 0,
        isRunning: false,
      };
    }

    const now = Date.now();
    const elapsed = now - this.startTime;
    const phase = ((now - this.lastTickTime) / this.intervalMs) % 1;

    return {
      ...this.clockState,
      timestamp: now,
      elapsed: elapsed,
      phase: phase,
      isRunning: true,
    };
  }

  /**
   * Get time until next tick
   * @returns {number} Milliseconds until next tick
   */
  getTimeUntilNextTick() {
    if (!this.isRunning) {
      return this.intervalMs;
    }

    const now = Date.now();
    const timeSinceLastTick = now - this.lastTickTime;
    const timeUntilNextTick = this.intervalMs - timeSinceLastTick;
    
    return Math.max(0, timeUntilNextTick);
  }

  /**
   * Wait for next tick
   * @returns {Promise<void>}
   */
  async waitForNextTick() {
    if (!this.isRunning) {
      await this.sleep(this.intervalMs);
      return;
    }

    const timeUntilNext = this.getTimeUntilNextTick();
    if (timeUntilNext > 0) {
      await this.sleep(timeUntilNext);
    }
  }

  /**
   * Set interval (restart clock if running)
   * @param {number} intervalMs - New interval in milliseconds
   */
  setInterval(intervalMs) {
    const wasRunning = this.isRunning;
    
    if (wasRunning) {
      this.stop();
    }

    this.intervalMs = intervalMs;

    if (wasRunning) {
      this.start(this.phaseOffset);
    }
  }

  /**
   * Sleep utility
   * @param {number} ms - Milliseconds to sleep
   */
  sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  /**
   * Align phase with another clock
   * @param {ClockSync} otherClock - Other clock to align with
   */
  alignWith(otherClock) {
    if (!otherClock.isRunning) {
      return;
    }

    const otherState = otherClock.getState();
    const targetPhase = otherState.phase;
    
    // Calculate when to start to align with target phase
    const now = Date.now();
    const timeUntilTargetPhase = (1 - targetPhase) * this.intervalMs;
    
    if (this.isRunning) {
      this.stop();
    }

    // Start with calculated phase offset
    setTimeout(() => {
      this.start(targetPhase);
    }, timeUntilTargetPhase);
  }

  /**
   * Create a synchronized callback that runs on clock ticks
   * @param {Function} callback - Callback to run on each tick
   * @param {number} everyNTicks - Run callback every N ticks (default: 1)
   * @returns {Function} Unsubscribe function
   */
  onTick(callback, everyNTicks = 1) {
    let tickCounter = 0;
    
    const handler = (tickData) => {
      tickCounter++;
      if (tickCounter >= everyNTicks) {
        tickCounter = 0;
        callback(tickData);
      }
    };

    this.on("tick", handler);

    // Return unsubscribe function
    return () => {
      this.off("tick", handler);
    };
  }
}

// Global clock instance (singleton)
let globalClock = null;

/**
 * Get global clock instance
 * @param {number} intervalMs - Interval in milliseconds (only used on first call)
 * @returns {ClockSync}
 */
export function getGlobalClock(intervalMs = 1000) {
  if (!globalClock) {
    globalClock = new ClockSync(intervalMs);
  }
  return globalClock;
}

/**
 * Reset global clock (for testing)
 */
export function resetGlobalClock() {
  if (globalClock) {
    globalClock.stop();
    globalClock = null;
  }
}
