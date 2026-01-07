# Build script for DWM3001C firmware
# Usage: .\build.ps1

Write-Host "Building DWM3001C firmware..." -ForegroundColor Green

# Get the project root directory
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = $projectRoot.Replace('\', '/')

# Ensure Docker is running
$dockerRunning = docker info 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Docker is not running. Please start Docker Desktop." -ForegroundColor Red
    exit 1
}

# Build the development environment if needed
Write-Host "Checking development environment..." -ForegroundColor Yellow
Get-Content Dockerfile | docker build -t uberi/qorvo-nrf52833-board -

if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Failed to build development environment." -ForegroundColor Red
    exit 1
}

# Build the firmware
Write-Host "Compiling firmware..." -ForegroundColor Yellow
docker run -v "${projectPath}:/project" uberi/qorvo-nrf52833-board /usr/local/segger_embedded_studio_V5.42a/bin/emBuild -config "Common" /project/dw3000_api.emProject

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "Output file: Output/Common/Exe/dw3000_api.hex" -ForegroundColor Cyan
} else {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

