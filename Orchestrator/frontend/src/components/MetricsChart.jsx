import React, { useMemo } from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from 'recharts';
import './MetricsChart.css';

/**
 * Metrics Chart Component
 * Displays time-series charts for RSSI, SNR, PER
 */
export default function MetricsChart({ data, metric = 'rssi_avg_dbm' }) {
  const chartData = useMemo(() => {
    if (!data || data.length === 0) return [];

    return data.map((item, index) => ({
      time: index,
      timestamp: new Date(item.timestamp).toLocaleTimeString(),
      nodeA: item.nodeA?.[metric] || 0,
      nodeB: item.nodeB?.[metric] || 0,
    }));
  }, [data, metric]);

  const getMetricLabel = () => {
    switch (metric) {
      case 'rssi_avg_dbm':
        return 'RSSI (dBm)';
      case 'snr_avg_db':
        return 'SNR (dB)';
      case 'per':
        return 'PER (%)';
      default:
        return metric;
    }
  };

  const getMetricColor = () => {
    switch (metric) {
      case 'rssi_avg_dbm':
        return { nodeA: '#2196f3', nodeB: '#4caf50' };
      case 'snr_avg_db':
        return { nodeA: '#ff9800', nodeB: '#9c27b0' };
      case 'per':
        return { nodeA: '#f44336', nodeB: '#e91e63' };
      default:
        return { nodeA: '#2196f3', nodeB: '#4caf50' };
    }
  };

  const colors = getMetricColor();

  return (
    <div className="metrics-chart">
      <h3 className="chart-title">{getMetricLabel()}</h3>
      <ResponsiveContainer width="100%" height={300}>
        <LineChart data={chartData}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis
            dataKey="timestamp"
            tick={{ fontSize: 12 }}
            interval="preserveStartEnd"
          />
          <YAxis tick={{ fontSize: 12 }} />
          <Tooltip />
          <Legend />
          <Line
            type="monotone"
            dataKey="nodeA"
            stroke={colors.nodeA}
            strokeWidth={2}
            dot={false}
            name="Node A"
          />
          <Line
            type="monotone"
            dataKey="nodeB"
            stroke={colors.nodeB}
            strokeWidth={2}
            dot={false}
            name="Node B"
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}

