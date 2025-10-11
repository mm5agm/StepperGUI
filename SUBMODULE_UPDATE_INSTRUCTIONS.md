# Submodule Update Instructions

## Quick Start (Windows Users)

**Double-click "Update Submodule.lnk" in Windows File Explorer**

⚠️ **IMPORTANT**: The shortcut must be run from **Windows File Explorer**, not from inside PlatformIO or VS Code!

### Why?
- PlatformIO/VS Code override .bat file associations and open them in the editor instead of executing them
- The .lnk shortcut bypasses this issue and runs the update script properly
- Running from Windows Explorer ensures the command prompt opens correctly

## What the Update Does

The update script will:
1. Pull the latest changes from the MagLoop_Common_Files repository
2. Update your local submodule to the latest version
3. Commit the submodule update to your project
4. Display status messages and wait for you to press a key

## Alternative Methods

If you prefer command line:
- **PowerShell**: `.\update_submodule.ps1`
- **Command Prompt**: `.\update_submodule.bat`  
- **Manual**: `git submodule update --remote MagLoop_Common_Files`

## Files Created

- `update_submodule.ps1` - PowerShell script
- `update_submodule.bat` - Batch file wrapper  
- `Update Submodule.lnk` - Windows shortcut (recommended for double-click)
- `create_shortcut.ps1` - Script to recreate the shortcut if needed

## Troubleshooting

**Problem**: Double-clicking opens files in PlatformIO editor
**Solution**: Use Windows File Explorer, not the PlatformIO file explorer

**Problem**: Nothing happens when double-clicking
**Solution**: Try right-click → "Run as administrator" on the shortcut