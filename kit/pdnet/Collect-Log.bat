@echo off
rem Copies the pdnet log to your Desktop to send after a test. Double-click to run.
rem Runs collect-log.ps1 from this folder, with no PowerShell settings to change.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0collect-log.ps1"
rem Keep the window open if the script stopped with an error.
if errorlevel 1 pause
