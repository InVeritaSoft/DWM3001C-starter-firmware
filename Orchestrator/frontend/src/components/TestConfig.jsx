import React from 'react';
import './TestConfig.css';

/**
 * Test Configuration Component
 * Displays current test configuration
 */
export default function TestConfig({ test, config }) {
  if (!test && !config) {
    return (
      <div className="test-config">
        <h3>Test Configuration</h3>
        <p className="no-config">No test configuration available</p>
      </div>
    );
  }

  const displayConfig = test?.config || config || {};

  return (
    <div className="test-config">
      <h3>Test Configuration</h3>
      {test && (
        <div className="config-section">
          <div className="config-row">
            <span className="config-label">Run Name:</span>
            <span className="config-value">{test.run_name}</span>
          </div>
          <div className="config-row">
            <span className="config-label">Distance:</span>
            <span className="config-value">{test.d_link} m</span>
          </div>
          <div className="config-row">
            <span className="config-label">Environment:</span>
            <span className="config-value">{test.env_type}</span>
          </div>
          <div className="config-row">
            <span className="config-label">Jammer:</span>
            <span className="config-value">{test.jammer_label}</span>
          </div>
        </div>
      )}
      <div className="config-section">
        <h4>UWB Settings</h4>
        <div className="config-grid">
          <div className="config-row">
            <span className="config-label">Channel:</span>
            <span className="config-value">{displayConfig.channel || 'N/A'}</span>
          </div>
          <div className="config-row">
            <span className="config-label">Data Rate:</span>
            <span className="config-value">{displayConfig.data_rate || 'N/A'}</span>
          </div>
          <div className="config-row">
            <span className="config-label">Preamble Length:</span>
            <span className="config-value">{displayConfig.preamble_len || 'N/A'}</span>
          </div>
          <div className="config-row">
            <span className="config-label">Payload Length:</span>
            <span className="config-value">{displayConfig.payload_len || 'N/A'}</span>
          </div>
          <div className="config-row">
            <span className="config-label">TX Power:</span>
            <span className="config-value">{displayConfig.tx_power_idx || 'N/A'}</span>
          </div>
          <div className="config-row">
            <span className="config-label">Packet Rate:</span>
            <span className="config-value">{displayConfig.pkt_rate_hz || 'N/A'} Hz</span>
          </div>
        </div>
      </div>
    </div>
  );
}

