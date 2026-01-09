# COM Port "Access Denied" Error - Fix

## Problem

**Error:** `Access to the path 'COM15' is denied`

**Cause:** Another program is already using COM15 (likely the serial monitor)

## Solution

### Option 1: Close Serial Monitor First (Recommended)

**Step 1:** Go to the terminal running the serial monitor
**Step 2:** Press `Ctrl+C` to stop it
**Step 3:** Now send commands:
```powershell
.\Scripts\send-command.ps1 COM15 PNG
```

### Option 2: Use One Terminal at a Time

**You cannot use the same COM port in two programs simultaneously.**

**Workflow:**
1. **Monitor mode:** Run `.\Scripts\serial-monitor.ps1 COM15` to see output
2. **Press Ctrl+C** to stop monitor
3. **Send command:** Run `.\Scripts\send-command.ps1 COM15 PNG`
4. **Restart monitor** if you want to see more output

### Option 3: Use Interactive Command Sender

The interactive command sender shows responses directly, so you don't need a separate monitor:

```powershell
# Close serial monitor first (Ctrl+C)
# Then run:
.\Scripts\interactive-command.ps1 COM15
```

This lets you:
- Type commands
- See responses immediately
- No need for separate monitor

## Quick Fix Steps

1. **Find the terminal with serial monitor**
2. **Press `Ctrl+C`** to stop it
3. **Run your command:**
   ```powershell
   .\Scripts\send-command.ps1 COM15 PNG
   ```

## Alternative: Check What's Using the Port

If you're not sure what's using the port:

```powershell
# List processes using COM ports (if you have Process Explorer or similar)
# Or simply close all PowerShell windows and start fresh
```

## Best Practice

**For monitoring and sending commands:**

**Use the interactive command sender** - it's designed for this:
```powershell
.\Scripts\interactive-command.ps1 COM15
```

This way you don't need two terminals and avoid port conflicts.
