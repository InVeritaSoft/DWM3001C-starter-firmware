import axios from 'axios';

const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:5000/api';

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000,
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
    const response = await api.get('/status');
    return response.data;
  },

  /**
   * Get latest stats from both nodes
   */
  async getStats() {
    const response = await api.get('/stats');
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
    const response = await api.get('/history', { params });
    return response.data;
  },

  /**
   * Get test plan
   */
  async getTestPlan() {
    const response = await api.get('/test-plan');
    return response.data;
  },

  /**
   * Start test
   */
  async startTest(test) {
    const response = await api.post('/test/start', { test });
    return response.data;
  },

  /**
   * Stop test
   */
  async stopTest() {
    const response = await api.post('/test/stop');
    return response.data;
  },
};

export default apiClient;

