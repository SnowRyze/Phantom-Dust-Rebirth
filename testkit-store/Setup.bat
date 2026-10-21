@echo off
rem Sets up pdnet for the Microsoft Store Phantom Dust. Double-click to run.
rem Runs store-setup.ps1 from this folder, with no PowerShell settings to change.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0store-setup.ps1"
rem Keep the window open if the script stopped with an error.
if errorlevel 1 pause
