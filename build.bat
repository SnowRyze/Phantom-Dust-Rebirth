@echo off
rem Builds everything: pdnet.dll, pdlaunch.exe and a ready-to-share kit, all in out\. See README.md.
rem   build.bat                          build, then make out\kit and out\PhantomDust-pdnet.zip
rem   build.bat experimental "notes"     the same, then save it as the next numbered Experimental kit
rem   build.bat promote N                rename Experimental N to the next Stable number
rem   build.bat public N                 publish Experimental N as the next Public, with private\public-links.ini
rem   build.bat conceptual "notes"       save the next numbered Conceptual kit (Conceptual branch; never published or updated)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" %*
if errorlevel 1 (
    echo.
    echo BUILD FAILED - see the message above.
    if "%~1"=="" pause
    exit /b 1
)
if "%~1"=="" pause
