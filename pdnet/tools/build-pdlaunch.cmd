@echo off
rem Builds tools\out\pdlaunch.exe (loads pdnet into an unmodified game, see pdlaunch.cpp). A windows program, not a console one:
rem it stays running in the background while the game is open, and a console program would show a window all that time.
setlocal
set OUT=%~dp0out
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.22000.0 >nul || exit /b 1
if not exist "%OUT%\obj" mkdir "%OUT%\obj"
pushd "%OUT%"
cl /nologo /std:c++20 /EHsc /MT /O2 /W3 /permissive- /utf-8 /DUNICODE /D_UNICODE /Fo"obj\\" /Fe"pdlaunch.exe" "%~dp0pdlaunch.cpp" /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup kernel32.lib psapi.lib ole32.lib user32.lib || (popd & echo BUILD FAILED & exit /b 1)
popd
echo Built %OUT%\pdlaunch.exe
