@echo off
rem Removes pdnet again: Phantom Dust starts normally (Xbox Live). Double-click to run.
rem Runs store-remove.ps1 from this folder, with no PowerShell settings to change.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0store-remove.ps1"
rem Keep the window open if the script stopped with an error.
if errorlevel 1 pause
