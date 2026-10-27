@echo off
rem Sets up pdnet for the Microsoft Store Phantom Dust. Double-click to run.
rem Runs pdnet\setup.ps1 (everything else in the kit is in the pdnet folder), with no PowerShell settings to change.
cd /d "%~dp0pdnet"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0pdnet\setup.ps1"
rem Keep the window open if the script stopped with an error.
if errorlevel 1 pause
