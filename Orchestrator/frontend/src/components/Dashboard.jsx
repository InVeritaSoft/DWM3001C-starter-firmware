import React, { useState, useEffect, useRef } from "react";
import { useRealtimeData } from "../hooks/useRealtimeData";
import apiClient from "../services/api";
import { useToast } from "./Toast";
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
  const { status, stats, connected, error, testReport, testRunning, resetTestState, clearError } =
    useRealtimeData();
  const { showSuccess, showError, clearAllToasts } = useToast();
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

  // Monitor node states - report is now auto-generated in real-time via useRealtimeData hook
  // No need to manually generate report when nodes stop - it updates automatically
  useEffect(() => {
    const nodeAState = status?.nodeA?.state;
    const nodeBState = status?.nodeB?.state;

    // Update previous states for reference
    prevStatesRef.current = {
      nodeA: nodeAState,
      nodeB: nodeBState,
    };
  }, [status?.nodeA?.state, status?.nodeB?.state]);

  const handleStartTest = async () => {
    try {
      // Send START command to both Node A and Node B simultaneously
      console.log("Starting test on both nodes...");
      const [resultA, resultB] = await Promise.allSettled([
        apiClient.startNode("A"),
        apiClient.startNode("B"),
      ]);

      const errors = [];
      if (resultA.status === "rejected") {
        const errorMsg =
          resultA.reason?.response?.data?.error ||
          resultA.reason?.response?.data?.message ||
          resultA.reason?.message ||
          String(resultA.reason);
        const errorDetails = resultA.reason?.response?.data?.details
          ? `\nDetails: ${JSON.stringify(resultA.reason.response.data.details, null, 2)}`
          : "";
        errors.push(`Node A: ${errorMsg}${errorDetails}`);
      }
      if (resultB.status === "rejected") {
        const errorMsg =
          resultB.reason?.response?.data?.error ||
          resultB.reason?.response?.data?.message ||
          resultB.reason?.message ||
          String(resultB.reason);
        const errorDetails = resultB.reason?.response?.data?.details
          ? `\nDetails: ${JSON.stringify(resultB.reason.response.data.details, null, 2)}`
          : "";
        errors.push(`Node B: ${errorMsg}${errorDetails}`);
      }

      if (errors.length > 0) {
        console.error("Failed to start test on some nodes:", errors);
        showError(`Failed to start test:\n${errors.join("\n")}`);
      } else {
        console.log("✓ Test started on both nodes");
        showSuccess("Test started on both nodes");
      }
    } catch (error) {
      console.error("Failed to start test:", error);
      showError(`Failed to start test: ${error.message || error}`);
    }
  };

  const handleStopTest = async () => {
    try {
      // Send STOP command to both Node A and Node B simultaneously
      console.log("Stopping test on both nodes...");
      const [resultA, resultB] = await Promise.allSettled([
        apiClient.stopNode("A"),
        apiClient.stopNode("B"),
      ]);

      const errors = [];
      if (resultA.status === "rejected") {
        const errorMsg =
          resultA.reason?.response?.data?.error ||
          resultA.reason?.response?.data?.message ||
          resultA.reason?.message ||
          String(resultA.reason);
        const errorDetails = resultA.reason?.response?.data?.details
          ? `\nDetails: ${JSON.stringify(resultA.reason.response.data.details, null, 2)}`
          : "";
        errors.push(`Node A: ${errorMsg}${errorDetails}`);
        console.error(
          "Node A stop error:",
          resultA.reason?.response?.data || resultA.reason,
        );
      }
      if (resultB.status === "rejected") {
        const errorMsg =
          resultB.reason?.response?.data?.error ||
          resultB.reason?.response?.data?.message ||
          resultB.reason?.message ||
          String(resultB.reason);
        const errorDetails = resultB.reason?.response?.data?.details
          ? `\nDetails: ${JSON.stringify(resultB.reason.response.data.details, null, 2)}`
          : "";
        errors.push(`Node B: ${errorMsg}${errorDetails}`);
        console.error(
          "Node B stop error:",
          resultB.reason?.response?.data || resultB.reason,
        );
      }

      if (errors.length > 0) {
        console.error("Failed to stop test on some nodes:", errors);
        showError(`Failed to stop test:\n${errors.join("\n")}`);
      } else {
        console.log("✓ Test stopped on both nodes");
        showSuccess("Test stopped on both nodes");
      }
    } catch (error) {
      console.error("Failed to stop test:", error);
      const errorMsg =
        error?.response?.data?.error ||
        error?.response?.data?.message ||
        error?.message ||
        String(error);
      showError(`Failed to stop test: ${errorMsg}`);
    }
  };

  const handleGenerateReport = async () => {
    // Optional: Still allow manual report generation from backend API
    // But primary report is now auto-generated in real-time
    setReportLoading(true);
    try {
      const report = await apiClient.generateReport();
      setGeneratedReport(report);
      showSuccess("Report generated from backend API");
    } catch (error) {
      console.error("Failed to generate report:", error);
      showError(`Failed to generate report: ${error.message || error}`);
    } finally {
      setReportLoading(false);
    }
  };

  const handleFullReset = async () => {
    try {
      // Call full reset API endpoint which stops orchestrator and resets nodes
      console.log("Performing full reset (stopping orchestrator and resetting nodes + UI)...");
      
      const result = await apiClient.fullReset();

      // Reset UI states regardless of firmware reset success
      // Clear chart data
      setChartData([]);
      
      // Clear generated report
      setGeneratedReport(null);
      
      // Reset test report and state via hook
      resetTestState();
      
      // Clear previous states ref
      prevStatesRef.current = { nodeA: null, nodeB: null };
      
      // Clear all error messages and notifications
      clearError();
      clearAllToasts();

      if (result.errors && result.errors.length > 0) {
        console.warn("Full reset completed with some errors:", result.errors);
        showError(`Full reset completed with errors:\n${result.errors.join("\n")}\n\nOrchestrator stopped and UI states reset.`);
      } else {
        console.log("✓ Full reset complete: Orchestrator stopped, firmware stats and UI states reset");
        showSuccess("Full reset complete: Orchestrator stopped, firmware stats and UI states reset");
      }
    } catch (error) {
      console.error("Failed to perform full reset:", error);
      const errorMsg =
        error?.response?.data?.error ||
        error?.response?.data?.message ||
        error?.message ||
        String(error);
      
      // Still reset UI states even if API call fails
      setChartData([]);
      setGeneratedReport(null);
      resetTestState();
      prevStatesRef.current = { nodeA: null, nodeB: null };
      
      // Clear existing errors/toasts before showing new error
      clearError();
      clearAllToasts();
      
      showError(`Failed to perform full reset: ${errorMsg}\n\nUI states have been reset.`);
    }
  };

  const handleCommandSent = (nodeId, command, result) => {
    console.log(`Node ${nodeId} command ${command} sent:`, result);
    // Optionally refresh stats after command
    if (command === "STAT") {
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
              style={{ marginBottom: "0.5rem" }}
            >
              Start Test (Both Nodes)
            </button>
            <button className="btn btn-danger" onClick={handleStopTest}>
              Stop Test (Both Nodes)
            </button>
            <button
              className="btn btn-warning"
              onClick={handleFullReset}
              style={{ marginTop: "0.5rem" }}
              title="Stop orchestrator (stops polling and cancels pending commands), reset statistics on both nodes (sends RST command), and clear all UI states (charts, reports, errors, notifications, toasts)"
            >
              Full Reset (Stop Orchestrator + Nodes + UI)
            </button>
            <button
              className="btn btn-secondary"
              onClick={handleGenerateReport}
              disabled={reportLoading}
              style={{ marginTop: "0.5rem", backgroundColor: "#9e9e9e" }}
              title="Generate report from backend API (report also updates automatically in real-time)"
            >
              {reportLoading ? "Generating..." : "Generate Report (API)"}
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

      {/* Auto-generated real-time report - updates automatically as test runs */}
      {(testReport || testRunning) && (
        <div className="test-report-section">
          <div className="report-header-indicator">
            {testRunning ? (
              <span className="report-status-badge running">
                🔄 Live Report (Updating in real-time...)
              </span>
            ) : testReport ? (
              <span className="report-status-badge stopped">
                ✓ Final Report
              </span>
            ) : (
              <span className="report-status-badge running">
                ⏳ Preparing report...
              </span>
            )}
          </div>
          {testReport ? (
            <TestReport report={testReport} />
          ) : testRunning ? (
            <div className="report-loading">
              <p>Initializing report... Waiting for stats...</p>
            </div>
          ) : null}
        </div>
      )}

      {/* Optional: Backend API generated report (for comparison) */}
      {generatedReport && (
        <div className="test-report-section">
          <h3>Backend API Report (Optional)</h3>
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
