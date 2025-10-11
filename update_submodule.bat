@echo off
title MagLoop Submodule Updater
echo ============================================
echo MagLoop Common Files Submodule Updater
echo ============================================
echo.
echo Current directory: %cd%
echo Script location: %~dp0
echo.
echo Running submodule update script...
echo.

powershell.exe -ExecutionPolicy Bypass -File "%~dp0update_submodule.ps1"

if %errorlevel% neq 0 (
    echo.
    echo ERROR: PowerShell script failed with error level %errorlevel%
    echo.
) else (
    echo.
    echo Script completed successfully!
    echo.
)

echo ============================================
echo Press any key to close this window...
pause > nul