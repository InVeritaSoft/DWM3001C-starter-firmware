import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { EventEmitter } from 'events';
import createCsvWriter from 'csv-writer';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

/**
 * CSV Logger
 * Logs test data to CSV files with all required fields per spec
 */
export class CSVLogger extends EventEmitter {
  constructor(logsDir = path.join(__dirname, '../../data/logs')) {
    super();
    this.logsDir = logsDir;
    this.currentFile = null;
    this.csvWriter = null;
    this.writeQueue = [];
    this.isWriting = false;

    // Ensure logs directory exists
    if (!fs.existsSync(this.logsDir)) {
      fs.mkdirSync(this.logsDir, { recursive: true });
    }
  }

  /**
   * Create new CSV file for a test run
   * @param {string} runName - Test run name
   * @returns {string} File path
   */
  createLogFile(runName) {
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const filename = `uwb_test_${runName}_${timestamp}.csv`;
    const filePath = path.join(this.logsDir, filename);

    // Define CSV headers per spec
    const csvWriter = createCsvWriter.createObjectCsvWriter({
      path: filePath,
      header: [
        { id: 'timestamp', title: 'timestamp' },
        { id: 'run_name', title: 'run_name' },
        { id: 'node_id', title: 'node_id' },
        { id: 'link_distance_m', title: 'link_distance_m' },
        { id: 'env_type', title: 'env_type' },
        { id: 'channel', title: 'channel' },
        { id: 'data_rate', title: 'data_rate' },
        { id: 'tx_power_idx', title: 'tx_power_idx' },
        { id: 'pkt_rate_hz', title: 'pkt_rate_hz' },
        { id: 'jammer_label', title: 'jammer_label' },
        { id: 'total_rx', title: 'total_rx' },
        { id: 'lost_pkts', title: 'lost_pkts' },
        { id: 'crc_err', title: 'crc_err' },
        { id: 'rssi_avg_dbm', title: 'rssi_avg_dbm' },
        { id: 'snr_avg_db', title: 'snr_avg_db' },
        { id: 'preamble_q_avg', title: 'preamble_q_avg' },
        { id: 'per', title: 'per' },
      ],
    });

    this.currentFile = filePath;
    this.csvWriter = csvWriter;
    this.emit('fileCreated', filePath);

    return filePath;
  }

  /**
   * Calculate Packet Error Rate (PER)
   * PER = (lost_pkts + crc_err) / (total_rx + lost_pkts + crc_err) * 100
   * @param {number} totalRx - Total received packets
   * @param {number} lostPkts - Lost packets
   * @param {number} crcErr - CRC errors
   * @returns {number} PER percentage
   */
  calculatePER(totalRx, lostPkts, crcErr) {
    const total = totalRx + lostPkts + crcErr;
    if (total === 0) {
      return 0;
    }
    return ((lostPkts + crcErr) / total) * 100;
  }

  /**
   * Log data row
   * @param {Object} data - Data object with all required fields
   */
  async logData(data) {
    // Ensure required fields
    const row = {
      timestamp: data.timestamp || new Date().toISOString(),
      run_name: data.run_name || '',
      node_id: data.node_id || '',
      link_distance_m: data.link_distance_m || 0,
      env_type: data.env_type || '',
      channel: data.channel || 0,
      data_rate: data.data_rate || '',
      tx_power_idx: data.tx_power_idx || 0,
      pkt_rate_hz: data.pkt_rate_hz || 0,
      jammer_label: data.jammer_label || '',
      total_rx: data.total_rx || 0,
      lost_pkts: data.lost_pkts || 0,
      crc_err: data.crc_err || 0,
      rssi_avg_dbm: data.rssi_avg_dbm || 0,
      snr_avg_db: data.snr_avg_db || 0,
      preamble_q_avg: data.preamble_q_avg || 0,
      per: data.per !== undefined 
        ? data.per 
        : this.calculatePER(data.total_rx || 0, data.lost_pkts || 0, data.crc_err || 0),
    };

    // Add to write queue
    this.writeQueue.push(row);

    // Process queue
    await this.processQueue();
  }

  /**
   * Process write queue (async-safe)
   */
  async processQueue() {
    if (this.isWriting || this.writeQueue.length === 0) {
      return;
    }

    if (!this.csvWriter) {
      throw new Error('No CSV file created. Call createLogFile() first.');
    }

    this.isWriting = true;

    try {
      const rows = this.writeQueue.splice(0);
      await this.csvWriter.writeRecords(rows);
      this.emit('dataLogged', rows);
    } catch (error) {
      this.emit('error', error);
      throw error;
    } finally {
      this.isWriting = false;

      // Process remaining items in queue
      if (this.writeQueue.length > 0) {
        setImmediate(() => this.processQueue());
      }
    }
  }

  /**
   * Get current log file path
   * @returns {string|null}
   */
  getCurrentFile() {
    return this.currentFile;
  }

  /**
   * List all log files
   * @returns {Array<string>}
   */
  listLogFiles() {
    try {
      const files = fs.readdirSync(this.logsDir)
        .filter(file => file.endsWith('.csv'))
        .map(file => path.join(this.logsDir, file))
        .sort()
        .reverse(); // Most recent first
      return files;
    } catch (error) {
      this.emit('error', error);
      return [];
    }
  }

  /**
   * Read log file
   * @param {string} filePath - Path to CSV file
   * @returns {Promise<Array<Object>>}
   */
  async readLogFile(filePath) {
    try {
      const csvParser = (await import('csv-parser')).default;
      const results = [];

      return new Promise((resolve, reject) => {
        fs.createReadStream(filePath)
          .pipe(csvParser())
          .on('data', (data) => results.push(data))
          .on('end', () => resolve(results))
          .on('error', reject);
      });
    } catch (error) {
      throw error;
    }
  }
}

