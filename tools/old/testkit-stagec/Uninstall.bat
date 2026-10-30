@echo off
rem Removes the pdnet test build and restores your original files. Double-click to run.
rem Runs uninstall.ps1 from this folder, with no PowerShell settings to change.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1"
rem Keep the window open if the script stopped with an error.
if errorlevel 1 pause
