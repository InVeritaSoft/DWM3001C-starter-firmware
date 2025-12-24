import React from 'react';
import './NodeStatus.css';

/**
 * Node Status Component
 * Displays node connection status and state
 */
export default function NodeStatus({ nodeId, state, connected, lastError }) {
  const getStateColor = (state) => {
    switch (state) {
      case 'RUNNING':
        return '#4caf50';
      case 'CONFIGURED':
        return '#2196f3';
      case 'STOPPED':
        return '#ff9800';
      case 'ERROR':
        return '#f44336';
      default:
        return '#9e9e9e';
    }
  };

  const getConnectionStatus = () => {
    if (connected) {
      return { text: 'Connected', color: '#4caf50' };
    }
    return { text: 'Disconnected', color: '#f44336' };
  };

  const connStatus = getConnectionStatus();

  return (
    <div className="node-status">
      <div className="node-header">
        <h3>Node {nodeId}</h3>
        <div
          className="status-indicator"
          style={{ backgroundColor: connStatus.color }}
          title={connStatus.text}
        />
      </div>
      <div className="node-info">
        <div className="info-row">
          <span className="info-label">State:</span>
          <span
            className="info-value state-value"
            style={{ color: getStateColor(state) }}
          >
            {state}
          </span>
        </div>
        <div className="info-row">
          <span className="info-label">Connection:</span>
          <span className="info-value" style={{ color: connStatus.color }}>
            {connStatus.text}
          </span>
        </div>
        {lastError && (
          <div className="info-row error-row">
            <span className="info-label">Error:</span>
            <span className="info-value error-value">{lastError}</span>
          </div>
        )}
      </div>
    </div>
  );
}

