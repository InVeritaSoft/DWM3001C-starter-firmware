import React, { useState } from "react";
import apiClient from "../services/api";
import { useToast } from "./Toast";
import "./NodeControls.css";

/**
 * Node Controls Component
 * Individual command buttons for each node
 */
export default function NodeControls({ nodeId, connected, onCommandSent }) {
  const { showError, showSuccess } = useToast();
  const [loading, setLoading] = useState({});
  const [lastResponse, setLastResponse] = useState(null);

  const handleCommand = async (command, apiMethod, params = {}) => {
    setLoading((prev) => ({ ...prev, [command]: true }));
    setLastResponse(null);

    try {
      let result;
      if (apiMethod === "sendCommand") {
        result = await apiClient.sendNodeCommand(
          nodeId,
          command,
          params.timeout || 3000,
        );
      } else if (apiMethod === "start") {
        result = await apiClient.startNode(nodeId);
      } else if (apiMethod === "stop") {
        result = await apiClient.stopNode(nodeId);
      } else if (apiMethod === "getStats") {
        result = await apiClient.getNodeStats(nodeId);
      } else if (apiMethod === "cfg") {
        result = await apiClient.cfgNode(nodeId);
      } else if (apiMethod === "nodeType") {
        result = await apiClient.nodeType(nodeId);
      } else if (apiMethod === "resetStats") {
        result = await apiClient.resetNodeStats(nodeId);
      } else if (apiMethod === "ping") {
        result = await apiClient.pingNode(nodeId);
      }

      setLastResponse({ command, result, success: true });

      // Show success toast with appropriate message
      const commandNames = {
        PNG: "PING",
        CFG: "CONFIG",
        NODE_TYPE: "NODE_TYPE",
        START: "START",
        STOP: "STOP",
        STAT: "STATS",
        RST: "RESET_STATS",
      };
      const commandName = commandNames[command] || command;
      showSuccess(`Node ${nodeId}: ${commandName} command successful`);

      if (onCommandSent) {
        onCommandSent(nodeId, command, result);
      }
    } catch (error) {
      // Extract error message from axios response if available
      const errorMessage =
        error.response?.data?.error ||
        error.response?.data?.message ||
        error.message ||
        String(error);
      const errorDetails = error.response?.data?.details
        ? `\nDetails: ${JSON.stringify(error.response.data.details, null, 2)}`
        : "";
      setLastResponse({ command, error: errorMessage, success: false });
      showError(
        `Node ${nodeId}: Command failed: ${errorMessage}${errorDetails}`,
      );
      console.error(
        `Node ${nodeId} command error:`,
        error.response?.data || error,
      );
    } finally {
      setLoading((prev) => ({ ...prev, [command]: false }));
    }
  };

  const buttonClass = (command) => {
    const baseClass = "node-btn";
    if (loading[command]) return `${baseClass} loading`;
    return baseClass;
  };

  return (
    <div className="node-controls">
      <h4>Node {nodeId} Controls</h4>
      <div className="node-buttons-grid">
        <button
          className={buttonClass("PING")}
          onClick={() => handleCommand("PNG", "ping")}
          disabled={loading["PING"]}
        >
          {loading["PING"] ? "..." : "PING"}
        </button>

        <button
          className={buttonClass("CFG")}
          onClick={() => handleCommand("CFG", "cfg")}
          disabled={loading["CFG"]}
        >
          {loading["CFG"] ? "..." : "CFG"}
        </button>

        <button
          className={buttonClass("NODE_TYPE")}
          onClick={() => handleCommand("NODE_TYPE", "nodeType")}
          disabled={loading["NODE_TYPE"]}
        >
          {loading["NODE_TYPE"] ? "..." : "NODE_TYPE"}
        </button>

        <button
          className={buttonClass("START")}
          onClick={() => handleCommand("START", "start")}
          disabled={loading["START"]}
        >
          {loading["START"] ? "..." : "START"}
        </button>

        <button
          className={buttonClass("STOP")}
          onClick={() => handleCommand("STOP", "stop")}
          disabled={loading["STOP"]}
        >
          {loading["STOP"] ? "..." : "STOP"}
        </button>

        <button
          className={buttonClass("STAT")}
          onClick={() => handleCommand("STAT", "getStats")}
          disabled={loading["STAT"]}
        >
          {loading["STAT"] ? "..." : "STAT"}
        </button>

        <button
          className={buttonClass("RST")}
          onClick={() => handleCommand("RST", "resetStats")}
          disabled={loading["RST"]}
        >
          {loading["RST"] ? "..." : "RST"}
        </button>
      </div>

      {lastResponse && (
        <div
          className={`command-response ${lastResponse.success ? "success" : "error"}`}
        >
          <strong>{lastResponse.command}:</strong>{" "}
          {lastResponse.success
            ? JSON.stringify(lastResponse.result, null, 2)
            : lastResponse.error}
        </div>
      )}
    </div>
  );
}
