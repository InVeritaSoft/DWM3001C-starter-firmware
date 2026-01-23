"""
Configuration Manager
Loads settings from YAML/JSON files and environment variables
"""

import os
import json
import yaml
from pathlib import Path
from typing import Dict, Any, Optional, List
from dotenv import load_dotenv


class Config:
    """Configuration Manager"""
    
    def __init__(self):
        self.settings: Optional[Dict[str, Any]] = None
        self.test_plan: Optional[Dict[str, Any]] = None
        self._base_dir = Path(__file__).parent.parent.parent
        
        # Load environment variables
        self._load_env()
    
    def _load_env(self) -> None:
        """Load environment variables from .env files"""
        # Try platform-specific .env files first
        env_files = [
            self._base_dir / '.env.linux',
            self._base_dir / '.env.windows',
            self._base_dir / '.env',
        ]
        
        for env_file in env_files:
            if env_file.exists():
                print(f"[Config] Loading environment from: {env_file}")
                load_dotenv(env_file)
                break
    
    def load_settings(self, settings_path: Optional[str] = None) -> Dict[str, Any]:
        """Load settings from YAML file"""
        if settings_path is None:
            settings_path = self._base_dir / "config" / "settings.yaml"
        else:
            settings_path = Path(settings_path)
        
        try:
            if not settings_path.exists():
                print(f"Settings file not found: {settings_path}, using defaults")
                self.settings = self._get_default_settings()
                return self.settings
            
            with open(settings_path, 'r') as f:
                self.settings = yaml.safe_load(f)
            return self.settings
            
        except Exception as error:
            print(f"Failed to load settings: {error}, using defaults")
            self.settings = self._get_default_settings()
            return self.settings
    
    def load_test_plan(self, test_plan_path: Optional[str] = None) -> Dict[str, Any]:
        """Load test plan from JSON file"""
        if test_plan_path is None:
            test_plan_path = self._base_dir / "config" / "testPlan.json"
        else:
            test_plan_path = Path(test_plan_path)
        
        try:
            if not test_plan_path.exists():
                print(f"Test plan file not found: {test_plan_path}, using empty test plan")
                self.test_plan = {'tests': []}
                return self.test_plan
            
            with open(test_plan_path, 'r') as f:
                self.test_plan = json.load(f)
            return self.test_plan
            
        except Exception as error:
            print(f"Failed to load test plan: {error}, using empty test plan")
            self.test_plan = {'tests': []}
            return self.test_plan
    
    def _get_default_settings(self) -> Dict[str, Any]:
        """Get default settings"""
        return {
            'serial': {
                'node_a_port': os.getenv('SERIAL_NODE_A_PORT', os.getenv('NODE_A_PORT', 'COM3')),
                'node_b_port': os.getenv('SERIAL_NODE_B_PORT', os.getenv('NODE_B_PORT', 'COM4')),
                'baudrate': int(os.getenv('SERIAL_BAUDRATE', os.getenv('BAUDRATE', '115200'))),
                'timeout': int(os.getenv('SERIAL_TIMEOUT', os.getenv('TIMEOUT', '5'))),
            },
            'test': {
                'poll_interval_seconds': 2,
                'default_test_duration_seconds': 30,
                'channel': 5,
                'data_rate': '6m8',
                'preamble_len': 128,
                'payload_len': 64,
                'tx_power_idx': 5,
                'pkt_rate_hz': 100,
            },
            'web': {
                'port': int(os.getenv('WEB_PORT', os.getenv('PORT', '5000'))),
                'host': os.getenv('WEB_HOST', os.getenv('HOST', '0.0.0.0')),
            },
            'swagger': {
                'title': 'UWB Test Orchestrator API',
                'version': '1.0.0',
                'description': 'API for controlling UWB DWM3001C test rig',
            },
        }
    
    def get_serial_config(self) -> Dict[str, Any]:
        """Get serial port configuration (prioritizes env vars)"""
        if not self.settings:
            self.load_settings()
        return self.settings.get('serial', {})
    
    def get_test_config(self) -> Dict[str, Any]:
        """Get test configuration (prioritizes env vars)"""
        if not self.settings:
            self.load_settings()
        return self.settings.get('test', {})
    
    def get_web_config(self) -> Dict[str, Any]:
        """Get web server configuration (prioritizes env vars)"""
        if not self.settings:
            self.load_settings()
        return self.settings.get('web', {})
    
    def get_swagger_config(self) -> Dict[str, Any]:
        """Get Swagger configuration"""
        if not self.settings:
            self.load_settings()
        return self.settings.get('swagger', {})
    
    def get_test_plan(self) -> List[Dict[str, Any]]:
        """Get test plan"""
        if not self.test_plan:
            self.load_test_plan()
        return self.test_plan.get('tests', [])
    
    def get_default_uwb_config(self) -> Dict[str, Any]:
        """Get default UWB configuration"""
        return {
            'channel': 5,
            'data_rate': '6m8',
            'preamble_len': 128,
            'payload_len': 64,
            'tx_power_idx': 5,
            'pkt_rate_hz': 100,
        }


# Singleton instance
_config_instance: Optional[Config] = None


def get_config() -> Config:
    """Get configuration instance (singleton)"""
    global _config_instance
    if _config_instance is None:
        _config_instance = Config()
    return _config_instance
