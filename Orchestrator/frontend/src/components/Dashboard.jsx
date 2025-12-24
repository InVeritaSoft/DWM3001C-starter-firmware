import React, { useState, useEffect } from "react";
import { useRealtimeData } from "../hooks/useRealtimeData";
import apiClient from "../services/api";
import NodeStatus from "./NodeStatus";
import MetricsChart from "./MetricsChart";
import TestConfig from "./TestConfig";
import TestReport from "./TestReport";
import "./Dashboard.css";

/**
 * Dashboard Component
 * Real-time monitoring dashboard
 */
export default function Dashboard() {
  const { status, stats, connected, error, testReport, testRunning } = useRealtimeData();
  const [testPlan, setTestPlan] = useState(null);
  const [currentTest, setCurrentTest] = useState(null);
  const [chartData, setChartData] = useState([]);

  useEffect(() => {
    // Load test plan
    apiClient.getTestPlan().then((data) => {
      setTestPlan(data);
      if (data.tests && data.tests.length > 0) {
        setCurrentTest(data.tests[0]);
      }
    });

    // Initial status is loaded via Socket.io in useRealtimeData hook
  }, []);

  // Update chart data when stats change
  useEffect(() => {
    if (stats.nodeA || stats.nodeB) {
      setChartData((prev) => {
        const newData = [
          ...prev,
          { ...stats, timestamp: new Date().toISOString() },
        ];
        // Keep last 50 data points
        return newData.slice(-50);
      });
    }
  }, [stats]);

  const handleStartTest = async () => {
    if (!currentTest) return;
    try {
      // API returns immediately, test starts in background
      // UI will update via Socket.io 'testStarted' event
      await apiClient.startTest(currentTest);
    } catch (error) {
      console.error("Failed to start test:", error);
      alert(`Failed to start test: ${error.message || error}`);
    }
  };

  const handleStopTest = async () => {
    try {
      // API returns immediately, test stops in background
      // UI will update via Socket.io 'testStopped' event
      await apiClient.stopTest();
    } catch (error) {
      console.error("Failed to stop test:", error);
      alert(`Failed to stop test: ${error.message || error}`);
    }
  };

  return (
    <div className="dashboard">
      <div className="dashboard-header">
        <h2>Real-time Dashboard</h2>
        <div className="connection-status">
          <span
            className={`status-dot ${connected ? "connected" : "disconnected"}`}
          />
          <span>{connected ? "Connected" : "Disconnected"}</span>
        </div>
      </div>

      {error && (
        <div className="error-banner">
          Error: {error.message || "Unknown error"}
        </div>
      )}

      <div className="dashboard-grid">
        <div className="dashboard-column">
          <NodeStatus
            nodeId="A"
            state={status?.nodeA?.state || "IDLE"}
            connected={status?.nodeA?.connected || false}
            lastError={status?.nodeA?.lastError}
          />
          <NodeStatus
            nodeId="B"
            state={status?.nodeB?.state || "IDLE"}
            connected={status?.nodeB?.connected || false}
            lastError={status?.nodeB?.lastError}
          />
        </div>

        <div className="dashboard-column">
          <TestConfig test={currentTest} />
          <div className="test-controls">
            <h3>Test Controls</h3>
            {!testRunning ? (
              <button
                className="btn btn-primary"
                onClick={handleStartTest}
                disabled={!currentTest || !connected}
              >
                Start Test
              </button>
            ) : (
              <button
                className="btn btn-danger"
                onClick={handleStopTest}
                disabled={!connected}
              >
                Stop Test
              </button>
            )}
          </div>
        </div>
      </div>

      <div className="metrics-section">
        <h3>Live Metrics</h3>
        <div className="metrics-grid">
          <div className="metric-card">
            <div className="metric-label">Node A RSSI</div>
            <div className="metric-value">
              {stats.nodeA?.rssi_avg_dbm?.toFixed(2) || "N/A"} dBm
            </div>
          </div>
          <div className="metric-card">
            <div className="metric-label">Node B RSSI</div>
            <div className="metric-value">
              {stats.nodeB?.rssi_avg_dbm?.toFixed(2) || "N/A"} dBm
            </div>
          </div>
          <div className="metric-card">
            <div className="metric-label">Node A SNR</div>
            <div className="metric-value">
              {stats.nodeA?.snr_avg_db?.toFixed(2) || "N/A"} dB
            </div>
          </div>
          <div className="metric-card">
            <div className="metric-label">Node B SNR</div>
            <div className="metric-value">
              {stats.nodeB?.snr_avg_db?.toFixed(2) || "N/A"} dB
            </div>
          </div>
          <div className="metric-card">
            <div className="metric-label">Node A PER</div>
            <div className="metric-value">
              {stats.nodeA?.per?.toFixed(2) || "N/A"} %
            </div>
          </div>
          <div className="metric-card">
            <div className="metric-label">Node B PER</div>
            <div className="metric-value">
              {stats.nodeB?.per?.toFixed(2) || "N/A"} %
            </div>
          </div>
        </div>
      </div>

      <div className="charts-section">
        <MetricsChart data={chartData} metric="rssi_avg_dbm" />
        <MetricsChart data={chartData} metric="snr_avg_db" />
        <MetricsChart data={chartData} metric="per" />
      </div>

      {testReport && (
        <div className="test-report-section">
          <TestReport report={testReport} />
        </div>
      )}
    </div>
  );
}
