#!/usr/bin/env python3
"""
Run Web Server
Simple script to start the web server
"""

import sys
from pathlib import Path

# Add src directory to path
sys.path.insert(0, str(Path(__file__).parent / "src"))

from main import main
import asyncio

if __name__ == "__main__":
    asyncio.run(main())
