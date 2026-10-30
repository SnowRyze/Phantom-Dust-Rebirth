@echo off
rem Builds everything: pdnet.dll, pdlaunch.exe and a ready-to-share kit, all in out\. See README.md.
rem   build.bat                          build, then make out\kit and out\PhantomDust-pdnet.zip
rem   build.bat experimental "notes"     the same, then save it as the next numbered Experimental kit
rem   build.bat promote N                rename Experimental N to the next Stable number
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" %*
if errorlevel 1 (
    echo.
    echo BUILD FAILED - see the message above.
    if "%~1"=="" pause
    exit /b 1
)
if "%~1"=="" pause
