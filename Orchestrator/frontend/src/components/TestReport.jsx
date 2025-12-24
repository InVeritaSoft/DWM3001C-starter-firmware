import React from "react";
import "./TestReport.css";

/**
 * TestReport Component
 * Displays package counts and statistics for a test run between Start and Stop
 */
export default function TestReport({ report }) {
  if (!report) {
    return null;
  }

  const formatDuration = (ms) => {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds % 60;
    if (minutes > 0) {
      return `${minutes}m ${remainingSeconds}s`;
    }
    return `${seconds}s`;
  };

  const formatDateTime = (date) => {
    return new Date(date).toLocaleString();
  };

  const formatDateForFilename = (date) => {
    return new Date(date).toISOString().replace(/[:.]/g, "-").slice(0, -5);
  };

  const exportJSON = () => {
    const dataStr = JSON.stringify(report, null, 2);
    const dataBlob = new Blob([dataStr], { type: "application/json" });
    const url = URL.createObjectURL(dataBlob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `test-report-${formatDateForFilename(
      report.startTime
    )}.json`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  const exportCSV = () => {
    // Create CSV content
    const rows = [
      ["Test Report Summary"],
      [],
      ["Start Time", formatDateTime(report.startTime)],
      ["End Time", formatDateTime(report.endTime)],
      ["Duration", formatDuration(report.duration)],
      [],
      ["Node A (TX) - Packages Sent"],
      ["Packets Sent", report.nodeA.packetsSent],
      ["Transmission Errors", report.nodeA.errors],
      [],
      ["Node B (RX) - Packages Received"],
      ["Packets Received", report.nodeB.packetsReceived],
      ["Packets Lost", report.nodeB.packetsLost],
      ["CRC Errors", report.nodeB.crcErrors],
      [],
      ["Link Quality Metrics"],
      ["Packet Loss Rate (%)", report.packetLossRate],
      ["Success Rate (%)", report.successRate],
      ["Total Packets (A→B)", report.nodeA.packetsSent],
    ];

    // Add signal quality if available
    if (report.nodeB.endStats) {
      rows.push([]);
      rows.push(["Average Signal Quality (Node B)"]);
      if (report.nodeB.endStats.rssi_avg_dbm !== undefined) {
        rows.push([
          "Average RSSI (dBm)",
          parseFloat(report.nodeB.endStats.rssi_avg_dbm || 0).toFixed(2),
        ]);
      }
      if (report.nodeB.endStats.snr_avg_db !== undefined) {
        rows.push([
          "Average SNR (dB)",
          parseFloat(report.nodeB.endStats.snr_avg_db || 0).toFixed(2),
        ]);
      }
    }

    // Convert to CSV format
    const csvContent = rows.map((row) => row.join(",")).join("\n");

    // Create and download file
    const dataBlob = new Blob([csvContent], {
      type: "text/csv;charset=utf-8;",
    });
    const url = URL.createObjectURL(dataBlob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `test-report-${formatDateForFilename(
      report.startTime
    )}.csv`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  };

  return (
    <div className="test-report">
      <div className="test-report-header">
        <div className="test-report-title-row">
          <h3>Test Report</h3>
          <div className="test-report-actions">
            <button
              className="btn btn-export btn-export-json"
              onClick={exportJSON}
            >
              Export JSON
            </button>
            <button
              className="btn btn-export btn-export-csv"
              onClick={exportCSV}
            >
              Export CSV
            </button>
          </div>
        </div>
        <div className="test-report-meta">
          <div className="meta-item">
            <span className="meta-label">Start Time:</span>
            <span className="meta-value">
              {formatDateTime(report.startTime)}
            </span>
          </div>
          <div className="meta-item">
            <span className="meta-label">End Time:</span>
            <span className="meta-value">{formatDateTime(report.endTime)}</span>
          </div>
          <div className="meta-item">
            <span className="meta-label">Duration:</span>
            <span className="meta-value">
              {formatDuration(report.duration)}
            </span>
          </div>
        </div>
      </div>

      <div className="test-report-content">
        <div className="report-section">
          <h4>Node A (TX) - Packages Sent</h4>
          <div className="report-grid">
            <div className="report-item">
              <span className="report-label">Packets Sent:</span>
              <span className="report-value highlight">
                {report.nodeA.packetsSent.toLocaleString()}
              </span>
            </div>
            <div className="report-item">
              <span className="report-label">Transmission Errors:</span>
              <span className="report-value">
                {report.nodeA.errors.toLocaleString()}
              </span>
            </div>
          </div>
        </div>

        <div className="report-section">
          <h4>Node B (RX) - Packages Received</h4>
          <div className="report-grid">
            <div className="report-item">
              <span className="report-label">Packets Received:</span>
              <span className="report-value highlight">
                {report.nodeB.packetsReceived.toLocaleString()}
              </span>
            </div>
            <div className="report-item">
              <span className="report-label">Packets Lost:</span>
              <span className="report-value error">
                {report.nodeB.packetsLost.toLocaleString()}
              </span>
            </div>
            <div className="report-item">
              <span className="report-label">CRC Errors:</span>
              <span className="report-value error">
                {report.nodeB.crcErrors.toLocaleString()}
              </span>
            </div>
          </div>
        </div>

        <div className="report-section">
          <h4>Link Quality Metrics</h4>
          <div className="report-grid">
            <div className="report-item">
              <span className="report-label">Packet Loss Rate:</span>
              <span className="report-value">{report.packetLossRate}%</span>
            </div>
            <div className="report-item">
              <span className="report-label">Success Rate:</span>
              <span className="report-value success">
                {report.successRate}%
              </span>
            </div>
            <div className="report-item">
              <span className="report-label">Total Packets (A→B):</span>
              <span className="report-value">
                {report.nodeA.packetsSent.toLocaleString()}
              </span>
            </div>
          </div>
        </div>

        {report.nodeB.endStats && (
          <div className="report-section">
            <h4>Average Signal Quality (Node B)</h4>
            <div className="report-grid">
              {report.nodeB.endStats.rssi_avg_dbm !== undefined && (
                <div className="report-item">
                  <span className="report-label">Average RSSI:</span>
                  <span className="report-value">
                    {parseFloat(
                      report.nodeB.endStats.rssi_avg_dbm || 0
                    ).toFixed(2)}{" "}
                    dBm
                  </span>
                </div>
              )}
              {report.nodeB.endStats.snr_avg_db !== undefined && (
                <div className="report-item">
                  <span className="report-label">Average SNR:</span>
                  <span className="report-value">
                    {parseFloat(report.nodeB.endStats.snr_avg_db || 0).toFixed(
                      2
                    )}{" "}
                    dB
                  </span>
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
