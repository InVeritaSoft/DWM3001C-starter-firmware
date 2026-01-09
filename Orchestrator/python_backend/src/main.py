"""
Main Entry Point
Starts the web server with uvicorn
"""

import asyncio
import signal
import sys
from pathlib import Path

# Add src directory to path
sys.path.insert(0, str(Path(__file__).parent))

from web.server import WebServer
import uvicorn


async def main():
    """Main entry point"""
    # Create server
    server = WebServer()
    
    # Setup graceful shutdown
    def signal_handler(signum, frame):
        print(f"\nSignal {signum} received, shutting down gracefully...")
        asyncio.create_task(server.stop())
        sys.exit(0)
    
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    
    # Start server (connect to nodes)
    await server.start()
    
    # Get web config
    web_config = server.config.get_web_config()
    port = web_config.get('port', 5000)
    host = web_config.get('host', '0.0.0.0')
    
    # Run uvicorn server
    config = uvicorn.Config(
        server.socket_app,
        host=host,
        port=port,
        log_level="info"
    )
    server_instance = uvicorn.Server(config)
    await server_instance.serve()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutting down...")
        sys.exit(0)
