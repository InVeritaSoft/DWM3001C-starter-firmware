import { useState, useEffect, useRef, useCallback } from "react";
import socketService from "../services/socket";

/**
 * Custom hook for real-time data
 * Manages Socket.io connection and real-time updates
 */
export function useRealtimeData() {
  const [status, setStatus] = useState(null);
  const [stats, setStats] = useState({ nodeA: null, nodeB: null });
  const [connected, setConnected] = useState(false);
  const [error, setError] = useState(null);
  const [testReport, setTestReport] = useState(null);
  const [testRunning, setTestRunning] = useState(false);
  const testStartStatsRef = useRef(null);
  const testStartTimeRef = useRef(null);

  // Calculate report from current stats (called whenever stats update)
  const calculateReport = useCallback(() => {
    try {
      if (!testStartStatsRef.current || !testStartTimeRef.current) {
        return;
      }

      const currentTime = new Date();
      // Always calculate duration from start time for real-time updates
      const duration = Math.max(0, currentTime - testStartTimeRef.current);

      // Safe value extraction with defaults
      const nodeAStats = stats.nodeA || {};
      const nodeBStats = stats.nodeB || {};
      const startNodeA = testStartStatsRef.current.nodeA || {};
      const startNodeB = testStartStatsRef.current.nodeB || {};

      // Calculate deltas, ensuring non-negative values
      const nodeATotalSent = Number(nodeAStats.total_sent) || 0;
      const startNodeATotalSent = Number(startNodeA.total_sent) || 0;
      const packetsSent = Math.max(0, nodeATotalSent - startNodeATotalSent);

      const nodeALastError = Number(nodeAStats.last_error) || 0;
      const startNodeALastError = Number(startNodeA.last_error) || 0;
      const errors = Math.max(0, nodeALastError - startNodeALastError);

      const nodeBTotalRx = Number(nodeBStats.total_rx) || 0;
      const startNodeBTotalRx = Number(startNodeB.total_rx) || 0;
      const packetsReceived = Math.max(0, nodeBTotalRx - startNodeBTotalRx);

      const nodeBLostPkts = Number(nodeBStats.lost_pkts) || 0;
      const startNodeBLostPkts = Number(startNodeB.lost_pkts) || 0;
      const packetsLost = Math.max(0, nodeBLostPkts - startNodeBLostPkts);

      const nodeBCrcErr = Number(nodeBStats.crc_err) || 0;
      const startNodeBCrcErr = Number(startNodeB.crc_err) || 0;
      const crcErrors = Math.max(0, nodeBCrcErr - startNodeBCrcErr);

      const report = {
        startTime: testStartTimeRef.current,
        endTime: testRunning ? null : currentTime,
        duration: duration,
        nodeA: {
          packetsSent: packetsSent,
          errors: errors,
          startStats: testStartStatsRef.current.nodeA,
          endStats: nodeAStats,
        },
        nodeB: {
          packetsReceived: packetsReceived,
          packetsLost: packetsLost,
          crcErrors: crcErrors,
          startStats: testStartStatsRef.current.nodeB,
          endStats: nodeBStats,
        },
      };

      // Calculate packet loss rate and success rate safely
      const totalSent = report.nodeA.packetsSent;
      const totalReceived = report.nodeB.packetsReceived;
      const totalLost = report.nodeB.packetsLost;
      
      report.packetLossRate =
        totalSent > 0 ? ((totalLost / totalSent) * 100).toFixed(2) : "0.00";
      report.successRate =
        totalSent > 0
          ? ((totalReceived / totalSent) * 100).toFixed(2)
          : "0.00";

      setTestReport(report);
    } catch (error) {
      console.error("[useRealtimeData] Error calculating report:", error);
      // Don't throw - just log the error to prevent interruptions
    }
  }, [stats, testRunning]);

  // Update report whenever stats change (real-time updates)
  useEffect(() => {
    if (testRunning && testStartStatsRef.current && testStartTimeRef.current) {
      calculateReport();
    }
  }, [
    stats.nodeA?.total_sent,
    stats.nodeA?.last_error,
    stats.nodeB?.total_rx,
    stats.nodeB?.lost_pkts,
    stats.nodeB?.crc_err,
    testRunning,
    calculateReport,
  ]);

  // Update duration in real-time every second when test is running
  // This effect triggers calculateReport to update duration while preserving stats
  useEffect(() => {
    if (!testRunning || !testStartTimeRef.current) {
      return;
    }

    const interval = setInterval(() => {
      if (testStartStatsRef.current && testStartTimeRef.current && testRunning) {
        // Trigger calculateReport which will update duration along with stats
        // This ensures duration updates smoothly without overwriting stats
        calculateReport();
      }
    }, 1000); // Update every second for smooth duration counter

    return () => clearInterval(interval);
  }, [testRunning, calculateReport]);

  useEffect(() => {
    // Connect to socket
    socketService.connect();

    // Status updates
    const handleStatus = (data) => {
      setStatus(data);
      // Sync testRunning from status if available
      if (data.testRunning !== undefined) {
        setTestRunning(data.testRunning);
      }
    };

    // Stats updates
    const handleStats = (data) => {
      if (data.node) {
        // Single node update
        setStats((prev) => ({
          ...prev,
          [data.node === "A" ? "nodeA" : "nodeB"]: data.stats,
        }));
      } else {
        // Both nodes update
        setStats(data);
      }
    };

    // Connection status
    const handleConnected = () => {
      setConnected(true);
      setError(null);
    };

    const handleDisconnected = () => {
      setConnected(false);
    };

    const handleError = (err) => {
      setError(err);
    };

    // Test start handler - capture initial stats
    const handleTestStarted = (test) => {
      setTestRunning(true);
      // Capture current stats as baseline - use functional update to get latest stats
      setStats((currentStats) => {
        testStartTimeRef.current = new Date();
        testStartStatsRef.current = {
          nodeA: currentStats.nodeA ? { ...currentStats.nodeA } : null,
          nodeB: currentStats.nodeB ? { ...currentStats.nodeB } : null,
        };
        // Initialize report immediately when test starts
        setTestReport({
          startTime: testStartTimeRef.current,
          endTime: null,
          duration: 0,
          nodeA: {
            packetsSent: 0,
            errors: 0,
            startStats: testStartStatsRef.current.nodeA,
            endStats: null,
          },
          nodeB: {
            packetsReceived: 0,
            packetsLost: 0,
            crcErrors: 0,
            startStats: testStartStatsRef.current.nodeB,
            endStats: null,
          },
          packetLossRate: "0.00",
          successRate: "0.00",
        });
        return currentStats; // Return unchanged stats
      });
    };

    // Test stop handler - finalize report
    const handleTestStopped = () => {
      setTestRunning(false);
      if (testStartStatsRef.current && testStartTimeRef.current) {
        calculateReport(); // Final calculation with endTime set
      }
    };

    // Node state change handler - update status in real-time
    const handleNodeStateChange = (data) => {
      setStatus((prev) => ({
        ...prev,
        [data.node === "A" ? "nodeA" : "nodeB"]: {
          ...prev?.[data.node === "A" ? "nodeA" : "nodeB"],
          state: data.newState,
        },
      }));
    };

    // Subscribe to events
    socketService.on("status", handleStatus);
    socketService.on("stats", handleStats);
    socketService.on("connected", handleConnected);
    socketService.on("disconnected", handleDisconnected);
    socketService.on("error", handleError);
    socketService.on("testStarted", handleTestStarted);
    socketService.on("testStopped", handleTestStopped);
    socketService.on("nodeStateChange", handleNodeStateChange);

    // Initial status
    socketService.on("status", handleStatus);

    // Cleanup
    return () => {
      socketService.off("status", handleStatus);
      socketService.off("stats", handleStats);
      socketService.off("connected", handleConnected);
      socketService.off("disconnected", handleDisconnected);
      socketService.off("error", handleError);
      socketService.off("testStarted", handleTestStarted);
      socketService.off("testStopped", handleTestStopped);
      socketService.off("nodeStateChange", handleNodeStateChange);
    };
  }, [calculateReport]);

  return {
    status,
    stats,
    connected,
    error,
    testReport,
    testRunning,
  };
}
