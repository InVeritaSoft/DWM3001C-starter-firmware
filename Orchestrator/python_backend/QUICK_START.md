# Quick Start - Python Backend

## 🚀 Get Running in 2 Minutes

### Linux (Raspberry Pi 5)

```bash
cd ~/projects/DWM3001C-starter-firmware/Orchestrator/python_backend

# One-command setup and run
chmod +x run_server.sh
./run_server.sh
```

### Windows

```powershell
cd C:\Users\lolibai\Documents\INVERITA\DWM3001C-starter-firmware\Orchestrator\python_backend

# One-command setup and run
powershell -ExecutionPolicy Bypass -File run_server.ps1
```

## ✅ Expected Output

```
[Config] Loading environment from: .env.linux
Connecting to Node A...
[RS485] Port /dev/ttyUSB0 opened at 115200 baud - ready to receive data
> Node A connected
Pinging Node A...
✓ Node A PING successful
✓ Node A is TX_V2

Connecting to Node B...
[RS485] Port /dev/ttyUSB1 opened at 115200 baud - ready to receive data
Node B connected
Pinging Node B...
✓ Node B PING successful
✓ Node B is RX_V2

Web server running on http://0.0.0.0:5000
Swagger UI available at http://0.0.0.0:5000/docs
INFO:     Uvicorn running on http://0.0.0.0:5000 (Press CTRL+C to quit)
```

## 🌐 Access Points

- **Web UI**: http://localhost:5000
- **API Docs**: http://localhost:5000/docs
- **API Status**: http://localhost:5000/api/status

## 🧪 Quick Test

```bash
# Test API
curl http://localhost:5000/api/status

# Should return:
# {
#   "nodeA": {"state": "IDLE", "connected": true, "lastError": null},
#   "nodeB": {"state": "IDLE", "connected": true, "lastError": null},
#   "testRunning": false
# }
```

## 🐛 Troubleshooting

### Issue: "Port /dev/ttyUSB0 not found"

```bash
# List available ports
ls -l /dev/ttyUSB*

# Update .env.linux with correct ports
nano ../.env.linux
```

### Issue: "Permission denied"

```bash
# Add user to dialout group (Linux)
sudo usermod -a -G dialout $USER
# Log out and log back in
```

### Issue: "Module not found"

```bash
# Reinstall dependencies
source venv/bin/activate  # Linux
# Or: .\venv\Scripts\Activate.ps1  # Windows
pip install -r requirements.txt
```

## 📚 Next Steps

1. **Test PING**: Check nodes respond to commands
2. **Run a test**: Use web UI or API to start a test
3. **View results**: Check CSV logs in `../data/logs/`
4. **Read docs**: See [README.md](README.md) for full documentation

## 🔄 Switch Back to Node.js

```bash
# Stop Python server (Ctrl+C)

# Start Node.js server
cd ..
npm run web
```

Both backends are **fully compatible** - use whichever you prefer!
