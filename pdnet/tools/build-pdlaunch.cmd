@echo off
rem Builds tools\out\pdlaunch.exe (loads pdnet into an unmodified game, see pdlaunch.cpp).
setlocal
set OUT=%~dp0out
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.22000.0 >nul || exit /b 1
if not exist "%OUT%\obj" mkdir "%OUT%\obj"
pushd "%OUT%"
cl /nologo /std:c++20 /EHsc /MT /O2 /W3 /permissive- /utf-8 /DUNICODE /D_UNICODE /Fo"obj\\" /Fe"pdlaunch.exe" "%~dp0pdlaunch.cpp" /link kernel32.lib psapi.lib ole32.lib || (popd & echo BUILD FAILED & exit /b 1)
popd
echo Built %OUT%\pdlaunch.exe
