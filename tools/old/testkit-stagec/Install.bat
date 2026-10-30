@echo off
rem Installs the pdnet test build into your Phantom Dust folder. Double-click to run.
rem Runs install.ps1 from this folder, with no PowerShell settings to change.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
rem Keep the window open if the script stopped with an error.
if errorlevel 1 pause
