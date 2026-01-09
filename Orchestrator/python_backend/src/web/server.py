"""
Web Server
FastAPI server with Socket.IO for real-time updates
"""

import asyncio
import socketio
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pathlib import Path
import sys

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from orchestrator import (
    Config, get_config,
    RS485Comm,
    NodeController,
    TestRunner,
    CSVLogger,
)
from .api import create_api_router


class WebServer:
    """Web Server with FastAPI and Socket.IO"""
    
    def __init__(self):
        self.app = FastAPI(
            title="UWB Test Orchestrator API",
            version="1.0.0",
            description="API for controlling UWB DWM3001C test rig"
        )
        
        # Socket.IO server
        self.sio = socketio.AsyncServer(
            async_mode='asgi',
            cors_allowed_origins='*'
        )
        self.socket_app = socketio.ASGIApp(self.sio, self.app)
        
        # Configuration
        self.config = get_config()
        self.config.load_settings()
        self.config.load_test_plan()
        
        # Initialize components
        self.csv_logger = CSVLogger()
        self.node_a: Optional[NodeController] = None
        self.node_b: Optional[NodeController] = None
        self.test_runner: Optional[TestRunner] = None
        
        self._setup_middleware()
        self._setup_routes()
        self._setup_socketio()
    
    def _setup_middleware(self) -> None:
        """Setup middleware"""
        # CORS
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )
    
    def _setup_routes(self) -> None:
        """Setup routes"""
        # Initialize nodes and test runner
        serial_config = self.config.get_serial_config()
        
        rs485_a = RS485Comm(
            serial_config['node_a_port'],
            serial_config['baudrate'],
            serial_config['timeout']
        )
        rs485_b = RS485Comm(
            serial_config['node_b_port'],
            serial_config['baudrate'],
            serial_config['timeout']
        )
        
        self.node_a = NodeController("A", rs485_a)
        self.node_b = NodeController("B", rs485_b)
        self.test_runner = TestRunner(
            self.node_a,
            self.node_b,
            self.csv_logger,
            self.config
        )
        
        # Disable interactive prompts when running from web server
        self.test_runner.skip_prompts = True
        
        # Setup event listeners for Socket.IO
        self._setup_event_listeners()
        
        # API routes
        api_router = create_api_router(
            self.node_a,
            self.node_b,
            self.test_runner,
            self.csv_logger,
            self.config
        )
        self.app.include_router(api_router, prefix="/api")
        
        # Serve static files from React build
        frontend_build = Path(__file__).parent.parent.parent.parent / "frontend" / "dist"
        if frontend_build.exists():
            self.app.mount("/static", StaticFiles(directory=str(frontend_build)), name="static")
        
        # Serve CSV files
        logs_dir = Path(__file__).parent.parent.parent.parent / "data" / "logs"
        if logs_dir.exists():
            self.app.mount("/data/logs", StaticFiles(directory=str(logs_dir)), name="logs")
        
        # Fallback to React app
        @self.app.get("/")
        async def root():
            index_path = frontend_build / "index.html"
            if index_path.exists():
                return FileResponse(str(index_path))
            return {"message": "UWB Test Orchestrator API", "docs": "/docs"}
    
    def _setup_socketio(self) -> None:
        """Setup Socket.IO"""
        @self.sio.event
        async def connect(sid, environ):
            print(f"Client connected: {sid}")
            
            # Send initial status
            await self.sio.emit('status', {
                'nodeA': {
                    'state': self.node_a.get_state().value if self.node_a else 'IDLE',
                    'connected': self.node_a.is_connected() if self.node_a else False,
                },
                'nodeB': {
                    'state': self.node_b.get_state().value if self.node_b else 'IDLE',
                    'connected': self.node_b.is_connected() if self.node_b else False,
                },
                'testRunning': self.test_runner.is_running if self.test_runner else False,
            }, room=sid)
        
        @self.sio.event
        async def disconnect(sid):
            print(f"Client disconnected: {sid}")
    
    def _setup_event_listeners(self) -> None:
        """Setup event listeners for real-time updates"""
        # Node A events
        def on_node_a_state_change(old_state, new_state, node_id):
            asyncio.create_task(self.sio.emit('nodeStateChange', {
                'node': 'A',
                'oldState': old_state.value,
                'newState': new_state.value,
                'nodeId': node_id,
            }))
        
        self.node_a.on_state_change(on_node_a_state_change)
        
        # Node B events
        def on_node_b_state_change(old_state, new_state, node_id):
            asyncio.create_task(self.sio.emit('nodeStateChange', {
                'node': 'B',
                'oldState': old_state.value,
                'newState': new_state.value,
                'nodeId': node_id,
            }))
        
        self.node_b.on_state_change(on_node_b_state_change)
        
        # Test runner events
        def on_stats(data):
            asyncio.create_task(self.sio.emit('stats', data))
        
        def on_test_started(test):
            asyncio.create_task(self.sio.emit('testStarted', test))
        
        def on_test_stopped():
            asyncio.create_task(self.sio.emit('testStopped'))
        
        def on_error(error):
            asyncio.create_task(self.sio.emit('error', {
                'message': str(error)
            }))
        
        self.test_runner.on('stats', on_stats)
        self.test_runner.on('test_started', on_test_started)
        self.test_runner.on('test_stopped', on_test_stopped)
        self.test_runner.on('error', on_error)
    
    async def start(self) -> None:
        """Start server and connect to nodes"""
        web_config = self.config.get_web_config()
        port = web_config.get('port', 5000)
        host = web_config.get('host', '0.0.0.0')
        
        # Connect to nodes
        try:
            print("Connecting to Node A...")
            try:
                await self.node_a.connect()
                print("> Node A connected")
                
                # Send PING to verify
                print("Pinging Node A...")
                try:
                    ping_result = await self.node_a.ping()
                    if ping_result:
                        print("✓ Node A PING successful")
                    else:
                        print("⚠️  Node A PING failed - firmware may not be responding")
                except Exception as ping_error:
                    print(f"⚠️  Node A PING error: {ping_error}")
                
                # Verify firmware type
                node_a_type = await self.node_a.get_firmware_node_type()
                if node_a_type:
                    print(f"✓ Node A is {node_a_type}")
                    if node_a_type not in ["TX_V2", "TX", "A"]:
                        print(f"⚠️  WARNING: Node A firmware is type '{node_a_type}' but expected 'TX'!")
                        print(f"⚠️  Rebuild Node A (TX) with: ./build-and-flash-tx.sh")
                else:
                    print(f"⚠️  WARNING: Could not determine Node A firmware type (NODE_TYPE command failed or returned null)")
                    print(f"⚠️  This indicates Node A firmware may not be running the orchestrator example")
                    print(f"⚠️  Rebuild and flash Node A (TX) with: ./build-and-flash-tx.sh")
                    
            except Exception as node_a_error:
                print(f"Failed to connect to Node A: {node_a_error}")
                print("Node A will not be available, but server will continue")
            
            print("Connecting to Node B...")
            try:
                await self.node_b.connect()
                print("Node B connected")
                
                # Send PING to verify
                print("Pinging Node B...")
                try:
                    ping_result = await self.node_b.ping()
                    if ping_result:
                        print("✓ Node B PING successful")
                    else:
                        print("⚠️  Node B PING failed - firmware may not be responding")
                except Exception as ping_error:
                    print(f"⚠️  Node B PING error: {ping_error}")
                
                # Verify firmware type
                node_b_type = await self.node_b.get_firmware_node_type()
                if node_b_type:
                    print(f"✓ Node B is {node_b_type}")
                    if node_b_type not in ["RX_V2", "RX", "B"]:
                        print(f"⚠️  WARNING: Node B firmware is type '{node_b_type}' but expected 'RX'!")
                        print(f"⚠️  Rebuild Node B (RX) with: ./build-and-flash-rx.sh")
                else:
                    print(f"⚠️  WARNING: Could not determine Node B firmware type (NODE_TYPE command failed or returned null)")
                    print(f"⚠️  This indicates Node B firmware may not be running the orchestrator example")
                    print(f"⚠️  Rebuild and flash Node B (RX) with: ./build-and-flash-rx.sh")
                    
            except Exception as node_b_error:
                print(f"Failed to connect to Node B: {node_b_error}")
                print("Node B will not be available, but server will continue")
                
        except Exception as error:
            print(f"Failed to connect to nodes: {error}")
            print("Server will start but nodes may not be available")
        
        print(f"Web server running on http://{host}:{port}")
        print(f"Swagger UI available at http://{host}:{port}/docs")
    
    async def stop(self) -> None:
        """Stop server and disconnect nodes"""
        if self.test_runner:
            await self.test_runner.stop_test()
        
        if self.node_a:
            await self.node_a.disconnect()
        
        if self.node_b:
            await self.node_b.disconnect()
        
        if self.csv_logger:
            self.csv_logger.close()


# Import Optional for type hints
from typing import Optional
