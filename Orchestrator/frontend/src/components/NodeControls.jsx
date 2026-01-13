import React, { useState } from 'react';
import apiClient from '../services/api';
import './NodeControls.css';

/**
 * Node Controls Component
 * Individual command buttons for each node
 */
export default function NodeControls({ nodeId, connected, onCommandSent }) {
  const [loading, setLoading] = useState({});
  const [lastResponse, setLastResponse] = useState(null);

  const handleCommand = async (command, apiMethod, params = {}) => {
    setLoading(prev => ({ ...prev, [command]: true }));
    setLastResponse(null);

    try {
      let result;
      if (apiMethod === 'sendCommand') {
        result = await apiClient.sendNodeCommand(nodeId, command, params.timeout || 3000);
      } else if (apiMethod === 'start') {
        result = await apiClient.startNode(nodeId);
      } else if (apiMethod === 'stop') {
        result = await apiClient.stopNode(nodeId);
      } else if (apiMethod === 'getStats') {
        result = await apiClient.getNodeStats(nodeId);
      } else if (apiMethod === 'init') {
        result = await apiClient.initNode(nodeId);
      } else if (apiMethod === 'resetStats') {
        result = await apiClient.resetNodeStats(nodeId);
      } else if (apiMethod === 'ping') {
        result = await apiClient.pingNode(nodeId);
      }

      setLastResponse({ command, result, success: true });
      if (onCommandSent) {
        onCommandSent(nodeId, command, result);
      }
    } catch (error) {
      setLastResponse({ command, error: error.message, success: false });
      alert(`Command failed: ${error.message}`);
    } finally {
      setLoading(prev => ({ ...prev, [command]: false }));
    }
  };

  const buttonClass = (command) => {
    const baseClass = 'node-btn';
    if (loading[command]) return `${baseClass} loading`;
    return baseClass;
  };

  return (
    <div className="node-controls">
      <h4>Node {nodeId} Controls</h4>
      <div className="node-buttons-grid">
        <button
          className={buttonClass('PING')}
          onClick={() => handleCommand('PNG', 'ping')}
          disabled={loading['PING']}
        >
          {loading['PING'] ? '...' : 'PING'}
        </button>
        
        <button
          className={buttonClass('INIT')}
          onClick={() => handleCommand('INIT', 'init')}
          disabled={loading['INIT']}
        >
          {loading['INIT'] ? '...' : 'INIT'}
        </button>
        
        <button
          className={buttonClass('START')}
          onClick={() => handleCommand('START', 'start')}
          disabled={loading['START']}
        >
          {loading['START'] ? '...' : 'START'}
        </button>
        
        <button
          className={buttonClass('STOP')}
          onClick={() => handleCommand('STOP', 'stop')}
          disabled={loading['STOP']}
        >
          {loading['STOP'] ? '...' : 'STOP'}
        </button>
        
        <button
          className={buttonClass('STAT')}
          onClick={() => handleCommand('STAT', 'getStats')}
          disabled={loading['STAT']}
        >
          {loading['STAT'] ? '...' : 'STAT'}
        </button>
        
        <button
          className={buttonClass('RST')}
          onClick={() => handleCommand('RST', 'resetStats')}
          disabled={loading['RST']}
        >
          {loading['RST'] ? '...' : 'RST'}
        </button>
      </div>
      
      {lastResponse && (
        <div className={`command-response ${lastResponse.success ? 'success' : 'error'}`}>
          <strong>{lastResponse.command}:</strong>{' '}
          {lastResponse.success 
            ? JSON.stringify(lastResponse.result, null, 2)
            : lastResponse.error
          }
        </div>
      )}
    </div>
  );
}
