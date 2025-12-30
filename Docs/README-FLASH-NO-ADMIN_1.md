# Flashing Without Administrator Privileges

## Quick Setup (One-Time)

Run the setup script **once** (may require admin for installation only):

```powershell
.\setup-usb.ps1
```

This will:

1. Install `usbipd-win` if not already installed (may need admin)
2. Find your J-Link device
3. Share it with WSL2

**After this one-time setup, you can flash without admin!**

## How It Works

1. **usbipd-win** shares USB devices from Windows to WSL2
2. Once shared, WSL2 can access the device without admin privileges
3. The flash script automatically uses WSL2 to flash via Docker

## Usage

After setup, just run:

```powershell
# Flash TX node
.\build-and-flash-tx.ps1

# Flash RX node
.\build-and-flash-rx.ps1

# Or use automated test
.\auto-test-tx-rx.ps1
```

**No admin privileges needed!**

## If USB Device Changes

If you:

- Unplug/replug the USB cable
- Connect a different board
- Restart your computer

Just run the setup script again:

```powershell
.\setup-usb.ps1
```

## Troubleshooting

### "USB device not accessible"

- Run `.\setup-usb.ps1` again
- Check device is connected: `usbipd list`
- Verify device is attached: Look for "Shared" status

### "usbipd-win not found"

- Install manually: `winget install usbipd-win`
- Or run `.\setup-usb.ps1` (it will install automatically)

### "Flash failed"

- Make sure board is powered on
- Check USB cable connection
- Verify Docker Desktop is running
- Try unplugging and replugging USB

## Manual Setup (Alternative)

If the automated script doesn't work:

1. Install usbipd-win:

   ```powershell
   winget install usbipd-win
   ```

2. List USB devices:

   ```powershell
   usbipd list
   ```

3. Find J-Link device (look for SEGGER or bus ID)

4. Attach to WSL2:

   ```powershell
   usbipd attach --wsl --busid <busid>
   ```

5. Now flash normally - no admin needed!
