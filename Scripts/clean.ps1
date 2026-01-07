# Clean build outputs
# Usage: .\clean.ps1

Write-Host "Cleaning build outputs..." -ForegroundColor Yellow

# Get the project root directory
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = $projectRoot.Replace('\', '/')

# Ensure Docker is running
$dockerRunning = docker info 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Docker is not running. Please start Docker Desktop." -ForegroundColor Red
    exit 1
}

# Clean build outputs
docker run -v "${projectPath}:/project" uberi/qorvo-nrf52833-board /usr/local/segger_embedded_studio_V5.42a/bin/emBuild -config "Common" -clean /project/dw3000_api.emProject

if ($LASTEXITCODE -eq 0) {
    Write-Host "Clean completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Clean failed!" -ForegroundColor Red
    exit 1
}

