# PowerShell Scripts for DWM3001C Firmware

This directory contains PowerShell scripts for building and flashing the DWM3001C firmware on Windows.

## Prerequisites

1. **Docker Desktop** installed and running (with WSL2 backend recommended for USB access)
2. **PowerShell** (comes with Windows 10/11)

## Quick Start

### Flash Node A (TX)

```powershell
.\build-and-flash-tx.ps1
```

This will:
1. Configure the firmware for TX variant
2. Build the firmware
3. Flash it to the connected board

### Flash Node B (RX)

```powershell
.\build-and-flash-rx.ps1
```

This will:
1. Configure the firmware for RX variant
2. Build the firmware
3. Flash it to the connected board

## Individual Scripts

### Configuration Scripts

- **`set-node-tx.ps1`** - Configure firmware for Node A (TX)
- **`set-node-rx.ps1`** - Configure firmware for Node B (RX)

### Build Scripts

- **`build.ps1`** - Build the firmware
- **`clean.ps1`** - Clean build outputs
- **`flash.ps1`** - Flash firmware to connected board

### Combined Scripts

- **`build-and-flash-tx.ps1`** - Configure, build, and flash TX variant
- **`build-and-flash-rx.ps1`** - Configure, build, and flash RX variant

## Usage Examples

### Step-by-step (Manual)

```powershell
# 1. Configure for TX
.\set-node-tx.ps1

# 2. Build
.\build.ps1

# 3. Flash
.\flash.ps1
```

### One-command (Automatic)

```powershell
# For Node A (TX)
.\build-and-flash-tx.ps1

# For Node B (RX)
.\build-and-flash-rx.ps1
```

## Troubleshooting

### Docker Not Running
```
Error: Docker is not running. Please start Docker Desktop.
```
**Solution:** Start Docker Desktop and wait for it to fully initialize.

### USB Device Access Issues
If flashing fails with USB device errors:
1. Ensure Docker Desktop is using WSL2 backend (Settings → General → Use WSL 2)
2. Try unplugging and replugging the USB cable
3. Ensure the board is connected to the J9 port (lower USB port)
4. Check that the board is powered on

### Build Fails
- Run `.\clean.ps1` to clean old build outputs
- Check that Docker has enough resources allocated (Settings → Resources)

### Permission Errors
If you get permission errors, run PowerShell as Administrator:
```powershell
# Right-click PowerShell → Run as Administrator
cd C:\Users\lolibai\Documents\INVERITA\DWM3001C-starter-firmware
.\build.ps1
```

## Notes

- The scripts automatically handle path conversion between Windows and Linux (Docker)
- USB device access requires Docker Desktop with WSL2 backend on Windows
- Make sure only one example is enabled at a time (scripts handle this automatically)
- The hex file is generated at: `Output\Common\Exe\dw3000_api.hex`

