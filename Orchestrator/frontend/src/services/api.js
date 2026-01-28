import axios from "axios";

const API_BASE_URL =
  import.meta.env.VITE_API_URL || "http://localhost:5000/api";

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 30000, // Increased to 30 seconds to allow for CONFIG and START commands that may take time
});

/**
 * API Client
 * Handles all REST API calls
 */
export const apiClient = {
  /**
   * Get current test status
   */
  async getStatus() {
    const response = await api.get("/status");
    return response.data;
  },

  /**
   * Get latest stats from both nodes
   */
  async getStats() {
    const response = await api.get("/stats");
    return response.data;
  },

  /**
   * Get historical data
   */
  async getHistory(page = 1, limit = 100, file = null) {
    const params = { page, limit };
    if (file) {
      params.file = file;
    }
    const response = await api.get("/history", { params });
    return response.data;
  },

  /**
   * Get test plan
   */
  async getTestPlan() {
    const response = await api.get("/test-plan");
    return response.data;
  },

  /**
   * Start test
   */
  async startTest(test) {
    const response = await api.post("/test/start", { test });
    return response.data;
  },

  /**
   * Stop test
   */
  async stopTest() {
    const response = await api.post("/test/stop");
    return response.data;
  },

  /**
   * Send command to specific node
   */
  async sendNodeCommand(nodeId, command, timeout) {
    const response = await api.post(`/nodes/${nodeId}/send`, {
      command,
      timeout,
    });
    return response.data;
  },

  /**
   * Start test on specific node
   */
  async startNode(nodeId) {
    const response = await api.post(`/nodes/${nodeId}/start`);
    return response.data;
  },

  /**
   * Stop test on specific node
   */
  async stopNode(nodeId) {
    const response = await api.post(`/nodes/${nodeId}/stop`);
    return response.data;
  },

  /**
   * Get stats from specific node
   */
  async getNodeStats(nodeId) {
    const response = await api.get(`/nodes/${nodeId}/stats`);
    return response.data;
  },

  /**
   * Configure specific node
   */
  async cfgNode(nodeId) {
    const response = await api.post(`/nodes/${nodeId}/cfg`);
    return response.data;
  },

  /**
   * Get node type from specific node
   */
  async nodeType(nodeId) {
    const response = await api.post(`/nodes/${nodeId}/node-type`);
    return response.data;
  },

  /**
   * Reset stats on specific node
   */
  async resetNodeStats(nodeId) {
    const response = await api.post(`/nodes/${nodeId}/reset-stats`);
    return response.data;
  },

  /**
   * Configure specific node
   */
  async configureNode(nodeId, config) {
    const response = await api.post(`/nodes/${nodeId}/configure`, config);
    return response.data;
  },

  /**
   * Ping specific node
   */
  async pingNode(nodeId) {
    const response = await api.post(`/nodes/${nodeId}/ping`);
    return response.data;
  },

  /**
   * Generate test report
   */
  async generateReport() {
    const response = await api.post("/test/report");
    return response.data;
  },
};

export default apiClient;
