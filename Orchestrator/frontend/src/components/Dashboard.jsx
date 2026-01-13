import React, { useState, useEffect, useRef } from "react";
import { useRealtimeData } from "../hooks/useRealtimeData";
import apiClient from "../services/api";
import NodeStatus from "./NodeStatus";
import NodeControls from "./NodeControls";
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
  const [generatedReport, setGeneratedReport] = useState(null);
  const [reportLoading, setReportLoading] = useState(false);
  const prevStatesRef = useRef({ nodeA: null, nodeB: null });

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

  // Monitor node states and generate report when both nodes stop
  useEffect(() => {
    const nodeAState = status?.nodeA?.state;
    const nodeBState = status?.nodeB?.state;
    const prevNodeAState = prevStatesRef.current.nodeA;
    const prevNodeBState = prevStatesRef.current.nodeB;

    // Check if both nodes just stopped (were running, now stopped)
    const bothJustStopped = 
      (prevNodeAState === 'RUNNING' && nodeAState === 'STOPPED') &&
      (prevNodeBState === 'RUNNING' && nodeBState === 'STOPPED');

    if (bothJustStopped) {
      handleGenerateReport();
    }

    // Update previous states
    prevStatesRef.current = {
      nodeA: nodeAState,
      nodeB: nodeBState
    };
  }, [status?.nodeA?.state, status?.nodeB?.state]);

  const handleStartTest = async () => {
    try {
      // Send START command to both Node A and Node B simultaneously
      console.log("Starting test on both nodes...");
      const [resultA, resultB] = await Promise.allSettled([
        apiClient.startNode("A"),
        apiClient.startNode("B")
      ]);

      const errors = [];
      if (resultA.status === 'rejected') {
        errors.push(`Node A: ${resultA.reason?.message || resultA.reason}`);
      }
      if (resultB.status === 'rejected') {
        errors.push(`Node B: ${resultB.reason?.message || resultB.reason}`);
      }

      if (errors.length > 0) {
        console.error("Failed to start test on some nodes:", errors);
        alert(`Failed to start test:\n${errors.join('\n')}`);
      } else {
        console.log("✓ Test started on both nodes");
      }
    } catch (error) {
      console.error("Failed to start test:", error);
      alert(`Failed to start test: ${error.message || error}`);
    }
  };

  const handleStopTest = async () => {
    try {
      // Send STOP command to both Node A and Node B simultaneously
      console.log("Stopping test on both nodes...");
      const [resultA, resultB] = await Promise.allSettled([
        apiClient.stopNode("A"),
        apiClient.stopNode("B")
      ]);

      const errors = [];
      if (resultA.status === 'rejected') {
        errors.push(`Node A: ${resultA.reason?.message || resultA.reason}`);
      }
      if (resultB.status === 'rejected') {
        errors.push(`Node B: ${resultB.reason?.message || resultB.reason}`);
      }

      if (errors.length > 0) {
        console.error("Failed to stop test on some nodes:", errors);
        alert(`Failed to stop test:\n${errors.join('\n')}`);
      } else {
        console.log("✓ Test stopped on both nodes");
      }
    } catch (error) {
      console.error("Failed to stop test:", error);
      alert(`Failed to stop test: ${error.message || error}`);
    }
  };

  const handleGenerateReport = async () => {
    setReportLoading(true);
    try {
      const report = await apiClient.generateReport();
      setGeneratedReport(report);
    } catch (error) {
      console.error("Failed to generate report:", error);
      alert(`Failed to generate report: ${error.message || error}`);
    } finally {
      setReportLoading(false);
    }
  };

  const handleCommandSent = (nodeId, command, result) => {
    console.log(`Node ${nodeId} command ${command} sent:`, result);
    // Optionally refresh stats after command
    if (command === 'STAT') {
      // Stats will be updated via realtime data hook
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
          <NodeControls
            nodeId="A"
            connected={status?.nodeA?.connected || false}
            onCommandSent={handleCommandSent}
          />
          <NodeStatus
            nodeId="B"
            state={status?.nodeB?.state || "IDLE"}
            connected={status?.nodeB?.connected || false}
            lastError={status?.nodeB?.lastError}
          />
          <NodeControls
            nodeId="B"
            connected={status?.nodeB?.connected || false}
            onCommandSent={handleCommandSent}
          />
        </div>

        <div className="dashboard-column">
          <TestConfig test={currentTest} />
          <div className="test-controls">
            <h3>Test Controls</h3>
            <button
              className="btn btn-primary"
              onClick={handleStartTest}
              disabled={testRunning}
              style={{ marginBottom: '0.5rem' }}
            >
              Start Test (Both Nodes)
            </button>
            <button
              className="btn btn-danger"
              onClick={handleStopTest}
              disabled={!testRunning}
            >
              Stop Test (Both Nodes)
            </button>
            <button
              className="btn btn-secondary"
              onClick={handleGenerateReport}
              disabled={reportLoading}
              style={{ marginTop: '0.5rem', backgroundColor: '#9e9e9e' }}
            >
              {reportLoading ? 'Generating...' : 'Generate Report'}
            </button>
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

      {generatedReport && (
        <div className="test-report-section">
          <h3>Generated Test Report</h3>
          <div className="report-content">
            <div className="report-section">
              <h4>Node A</h4>
              <pre>{JSON.stringify(generatedReport.nodeA, null, 2)}</pre>
            </div>
            <div className="report-section">
              <h4>Node B</h4>
              <pre>{JSON.stringify(generatedReport.nodeB, null, 2)}</pre>
            </div>
            <div className="report-section">
              <h4>Timestamp</h4>
              <p>{new Date(generatedReport.timestamp).toLocaleString()}</p>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
