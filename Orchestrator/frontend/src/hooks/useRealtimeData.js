import { useState, useEffect, useRef } from "react";
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
      // Capture current stats as baseline
      testStartTimeRef.current = new Date();
      testStartStatsRef.current = {
        nodeA: stats.nodeA ? { ...stats.nodeA } : null,
        nodeB: stats.nodeB ? { ...stats.nodeB } : null,
      };
      setTestReport(null); // Clear previous report
    };

    // Test stop handler - calculate report
    const handleTestStopped = () => {
      setTestRunning(false);
      if (testStartStatsRef.current && testStartTimeRef.current) {
        // Calculate delta between start and stop
        const endTime = new Date();
        const duration = endTime - testStartTimeRef.current;

        const report = {
          startTime: testStartTimeRef.current,
          endTime: endTime,
          duration: duration,
          nodeA: {
            packetsSent: stats.nodeA?.total_sent
              ? stats.nodeA.total_sent -
                (testStartStatsRef.current.nodeA?.total_sent || 0)
              : 0,
            errors: stats.nodeA?.last_error
              ? stats.nodeA.last_error -
                (testStartStatsRef.current.nodeA?.last_error || 0)
              : 0,
            startStats: testStartStatsRef.current.nodeA,
            endStats: stats.nodeA,
          },
          nodeB: {
            packetsReceived: stats.nodeB?.total_rx
              ? stats.nodeB.total_rx -
                (testStartStatsRef.current.nodeB?.total_rx || 0)
              : 0,
            packetsLost: stats.nodeB?.lost_pkts
              ? stats.nodeB.lost_pkts -
                (testStartStatsRef.current.nodeB?.lost_pkts || 0)
              : 0,
            crcErrors: stats.nodeB?.crc_err
              ? stats.nodeB.crc_err -
                (testStartStatsRef.current.nodeB?.crc_err || 0)
              : 0,
            startStats: testStartStatsRef.current.nodeB,
            endStats: stats.nodeB,
          },
        };

        // Calculate packet loss rate
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
  }, [stats]);

  return {
    status,
    stats,
    connected,
    error,
    testReport,
    testRunning,
  };
}
