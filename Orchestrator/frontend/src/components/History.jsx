import React, { useState, useEffect } from 'react';
import apiClient from '../services/api';
import MetricsChart from './MetricsChart';
import './History.css';

/**
 * History Component
 * Historical data view with CSV data table and charts
 */
export default function History() {
  const [logFiles, setLogFiles] = useState([]);
  const [selectedFile, setSelectedFile] = useState(null);
  const [data, setData] = useState([]);
  const [loading, setLoading] = useState(false);
  const [page, setPage] = useState(1);
  const [limit] = useState(100);
  const [filter, setFilter] = useState({ nodeId: '', dateFrom: '', dateTo: '' });

  useEffect(() => {
    loadLogFiles();
  }, []);

  useEffect(() => {
    if (selectedFile) {
      loadData(selectedFile);
    }
  }, [selectedFile, page]);

  const loadLogFiles = async () => {
    try {
      const response = await apiClient.getHistory();
      setLogFiles(response.files || []);
      if (response.files && response.files.length > 0 && !selectedFile) {
        setSelectedFile(response.files[0]);
      }
    } catch (error) {
      console.error('Failed to load log files:', error);
    }
  };

  const loadData = async (file) => {
    setLoading(true);
    try {
      const response = await apiClient.getHistory(page, limit, file);
      setData(response.data || []);
    } catch (error) {
      console.error('Failed to load data:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleExport = () => {
    if (!selectedFile) return;

    const link = document.createElement('a');
    link.href = `/data/logs/${selectedFile.split('/').pop()}`;
    link.download = selectedFile.split('/').pop();
    link.click();
  };

  const filteredData = data.filter((row) => {
    if (filter.nodeId && row.node_id !== filter.nodeId) return false;
    if (filter.dateFrom && row.timestamp < filter.dateFrom) return false;
    if (filter.dateTo && row.timestamp > filter.dateTo) return false;
    return true;
  });

  // Prepare chart data
  const chartData = filteredData.map((row) => ({
    timestamp: row.timestamp,
    nodeA: row.node_id === 'A' ? row : null,
    nodeB: row.node_id === 'B' ? row : null,
  }));

  return (
    <div className="history">
      <div className="history-header">
        <h2>Historical Data</h2>
        <div className="history-controls">
          <select
            value={selectedFile || ''}
            onChange={(e) => setSelectedFile(e.target.value)}
            className="file-select"
          >
            <option value="">Select log file...</option>
            {logFiles.map((file) => (
              <option key={file} value={file}>
                {file.split('/').pop()}
              </option>
            ))}
          </select>
          <button className="btn btn-secondary" onClick={handleExport} disabled={!selectedFile}>
            Export CSV
          </button>
        </div>
      </div>

      {selectedFile && (
        <>
          <div className="filters">
            <input
              type="text"
              placeholder="Filter by Node ID (A/B)"
              value={filter.nodeId}
              onChange={(e) => setFilter({ ...filter, nodeId: e.target.value })}
              className="filter-input"
            />
            <input
              type="datetime-local"
              placeholder="From Date"
              value={filter.dateFrom}
              onChange={(e) => setFilter({ ...filter, dateFrom: e.target.value })}
              className="filter-input"
            />
            <input
              type="datetime-local"
              placeholder="To Date"
              value={filter.dateTo}
              onChange={(e) => setFilter({ ...filter, dateTo: e.target.value })}
              className="filter-input"
            />
          </div>

          {loading ? (
            <div className="loading">Loading data...</div>
          ) : (
            <>
              <div className="charts-section">
                <MetricsChart data={chartData} metric="rssi_avg_dbm" />
                <MetricsChart data={chartData} metric="snr_avg_db" />
                <MetricsChart data={chartData} metric="per" />
              </div>

              <div className="data-table-container">
                <table className="data-table">
                  <thead>
                    <tr>
                      <th>Timestamp</th>
                      <th>Node</th>
                      <th>RSSI (dBm)</th>
                      <th>SNR (dB)</th>
                      <th>PER (%)</th>
                      <th>Total RX</th>
                      <th>Lost Pkts</th>
                      <th>CRC Err</th>
                    </tr>
                  </thead>
                  <tbody>
                    {filteredData.map((row, index) => (
                      <tr key={index}>
                        <td>{new Date(row.timestamp).toLocaleString()}</td>
                        <td>{row.node_id}</td>
                        <td>{parseFloat(row.rssi_avg_dbm || 0).toFixed(2)}</td>
                        <td>{parseFloat(row.snr_avg_db || 0).toFixed(2)}</td>
                        <td>{parseFloat(row.per || 0).toFixed(2)}</td>
                        <td>{row.total_rx || 0}</td>
                        <td>{row.lost_pkts || 0}</td>
                        <td>{row.crc_err || 0}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
                {filteredData.length === 0 && (
                  <div className="no-data">No data available</div>
                )}
              </div>
            </>
          )}
        </>
      )}
    </div>
  );
}

