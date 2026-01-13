#!/bin/bash
# Restart the Python backend server

cd "$(dirname "$0")"

# Kill any existing server
pkill -f "python.*run_server.py" 2>/dev/null
sleep 1

# Activate virtual environment and start server
source venv/bin/activate
python run_server.py
