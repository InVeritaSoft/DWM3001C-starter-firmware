# Run UWB Test Orchestrator Web Server (Python Backend)
# PowerShell script for Windows

$ErrorActionPreference = "Stop"

# Change to script directory
Set-Location $PSScriptRoot

# Check if virtual environment exists
if (!(Test-Path "venv")) {
    Write-Host "Virtual environment not found. Creating..." -ForegroundColor Yellow
    python -m venv venv
    Write-Host "Installing dependencies..." -ForegroundColor Yellow
    .\venv\Scripts\Activate.ps1
    pip install -r requirements.txt
} else {
    .\venv\Scripts\Activate.ps1
}

# Run server
python run_server.py
