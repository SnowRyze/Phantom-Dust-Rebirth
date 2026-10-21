@echo off
rem Puts Phantom Dust back on Xbox Live and Teredo: removes every pdnet change. Double-click to run.
rem Runs restore-xbl.ps1 from this folder, with no PowerShell settings to change.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore-xbl.ps1"
rem Keep the window open if the script stopped with an error.
if errorlevel 1 pause
