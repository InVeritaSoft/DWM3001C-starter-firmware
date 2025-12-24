# List all available COM ports
# Usage: .\list-com-ports.ps1

Write-Host "=== Available COM Ports ===" -ForegroundColor Cyan
Write-Host ""

$ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object

if ($ports.Count -eq 0) {
    Write-Host "No COM ports found" -ForegroundColor Yellow
} else {
    Write-Host "Found $($ports.Count) COM port(s):" -ForegroundColor Green
    foreach ($port in $ports) {
        Write-Host "  $port" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "Current configuration:" -ForegroundColor Cyan
Write-Host "  Node A (TX): COM17" -ForegroundColor White
Write-Host "  Node B (RX): COM18" -ForegroundColor White
Write-Host ""
Write-Host "To change ports, edit:" -ForegroundColor Yellow
Write-Host "  Orchestrator/config/settings.yaml" -ForegroundColor White
Write-Host "  .env.win (if exists)" -ForegroundColor White
