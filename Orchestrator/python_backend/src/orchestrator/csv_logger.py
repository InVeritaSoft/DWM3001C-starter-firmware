"""
CSV Logger
Logs test data to CSV files with all required fields per spec
"""

import csv
import os
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Any, Optional
import asyncio


class CSVLogger:
    """CSV Logger for test data"""
    
    def __init__(self, logs_dir: Optional[str] = None):
        if logs_dir is None:
            # Default to ../../data/logs relative to this file
            logs_dir = Path(__file__).parent.parent.parent.parent / "data" / "logs"
        
        self.logs_dir = Path(logs_dir)
        self.current_file: Optional[Path] = None
        self.csv_writer: Optional[csv.DictWriter] = None
        self.file_handle: Optional[Any] = None
        self.write_queue: List[Dict[str, Any]] = []
        self.is_writing = False
        self._lock = asyncio.Lock()
        
        # Ensure logs directory exists
        self.logs_dir.mkdir(parents=True, exist_ok=True)
    
    def create_log_file(self, run_name: str) -> str:
        """
        Create new CSV file for a test run
        
        Args:
            run_name: Test run name
            
        Returns:
            File path
        """
        timestamp = datetime.now().isoformat().replace(':', '-').replace('.', '-')
        filename = f"uwb_test_{run_name}_{timestamp}.csv"
        file_path = self.logs_dir / filename
        
        # Close previous file if open
        if self.file_handle:
            self.file_handle.close()
        
        # Open new file
        self.file_handle = open(file_path, 'w', newline='')
        
        # Define CSV headers per spec
        fieldnames = [
            'timestamp',
            'run_name',
            'node_id',
            'link_distance_m',
            'env_type',
            'channel',
            'data_rate',
            'tx_power_idx',
            'pkt_rate_hz',
            'jammer_label',
            'total_rx',
            'lost_pkts',
            'crc_err',
            'rssi_avg_dbm',
            'snr_avg_db',
            'preamble_q_avg',
            'per',
        ]
        
        self.csv_writer = csv.DictWriter(self.file_handle, fieldnames=fieldnames)
        self.csv_writer.writeheader()
        self.file_handle.flush()
        
        self.current_file = file_path
        print(f"[CSVLogger] Created log file: {file_path}")
        
        return str(file_path)
    
    @staticmethod
    def calculate_per(total_rx: int, lost_pkts: int, crc_err: int) -> float:
        """
        Calculate Packet Error Rate (PER)
        PER = (lost_pkts + crc_err) / (total_rx + lost_pkts + crc_err) * 100
        """
        total = total_rx + lost_pkts + crc_err
        if total == 0:
            return 0.0
        return ((lost_pkts + crc_err) / total) * 100
    
    async def log_data(self, data: Dict[str, Any]) -> None:
        """
        Log data row
        
        Args:
            data: Data dict with required fields
        """
        # Ensure required fields
        row = {
            'timestamp': data.get('timestamp', datetime.now().isoformat()),
            'run_name': data.get('run_name', ''),
            'node_id': data.get('node_id', ''),
            'link_distance_m': data.get('link_distance_m', 0),
            'env_type': data.get('env_type', ''),
            'channel': data.get('channel', 0),
            'data_rate': data.get('data_rate', ''),
            'tx_power_idx': data.get('tx_power_idx', 0),
            'pkt_rate_hz': data.get('pkt_rate_hz', 0),
            'jammer_label': data.get('jammer_label', ''),
            'total_rx': data.get('total_rx', 0),
            'lost_pkts': data.get('lost_pkts', 0),
            'crc_err': data.get('crc_err', 0),
            'rssi_avg_dbm': data.get('rssi_avg_dbm', 0),
            'snr_avg_db': data.get('snr_avg_db', 0),
            'preamble_q_avg': data.get('preamble_q_avg', 0),
            'per': data.get('per', self.calculate_per(
                data.get('total_rx', 0),
                data.get('lost_pkts', 0),
                data.get('crc_err', 0)
            )),
        }
        
        # Add to write queue
        self.write_queue.append(row)
        
        # Process queue
        await self._process_queue()
    
    async def _process_queue(self) -> None:
        """Process write queue (async-safe)"""
        if self.is_writing or not self.write_queue:
            return
        
        if not self.csv_writer:
            raise Exception('No CSV file created. Call create_log_file() first.')
        
        async with self._lock:
            if self.is_writing or not self.write_queue:
                return
            
            self.is_writing = True
            
            try:
                rows = self.write_queue.copy()
                self.write_queue.clear()
                
                # Write rows
                for row in rows:
                    self.csv_writer.writerow(row)
                
                self.file_handle.flush()
                
            except Exception as error:
                print(f"[CSVLogger] Error writing to CSV: {error}")
                raise
            finally:
                self.is_writing = False
                
                # Process remaining items in queue
                if self.write_queue:
                    asyncio.create_task(self._process_queue())
    
    def get_current_file(self) -> Optional[str]:
        """Get current log file path"""
        return str(self.current_file) if self.current_file else None
    
    def list_log_files(self) -> List[str]:
        """List all log files"""
        try:
            files = sorted(
                [str(f) for f in self.logs_dir.glob('*.csv')],
                reverse=True  # Most recent first
            )
            return files
        except Exception as error:
            print(f"[CSVLogger] Error listing log files: {error}")
            return []
    
    async def read_log_file(self, file_path: str) -> List[Dict[str, Any]]:
        """Read log file"""
        try:
            results = []
            with open(file_path, 'r', newline='') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    results.append(row)
            return results
        except Exception as error:
            raise Exception(f"Failed to read log file: {error}")
    
    def close(self) -> None:
        """Close current log file"""
        if self.file_handle:
            self.file_handle.close()
            self.file_handle = None
            self.csv_writer = None
