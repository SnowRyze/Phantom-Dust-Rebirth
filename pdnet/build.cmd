@echo off
rem Builds pdnet as build\Microsoft.Xbox.Services.dll (x64, static CRT so it needs nothing from the package).
setlocal
set ROOT=%~dp0
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (
    echo Visual Studio C++ tools not found. Install the "Desktop development with C++" workload.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.22000.0 >nul || exit /b 1

if not exist "%ROOT%build" mkdir "%ROOT%build"
pushd "%ROOT%build"
cl /nologo /std:c++20 /EHsc /MT /O2 /Zi /W3 /permissive- /bigobj /utf-8 ^
   /DUNICODE /D_UNICODE ^
   "%ROOT%src\dllmain.cpp" "%ROOT%src\hooks.cpp" "%ROOT%src\gamesave.cpp" "%ROOT%src\log.cpp" ^
   "%ROOT%src\config.cpp" "%ROOT%src\net.cpp" "%ROOT%src\sda.cpp" ^
   /LD /Fe:Microsoft.Xbox.Services.dll ^
   /link /DEF:"%ROOT%src\pdnet.def" /DEBUG /OPT:REF /OPT:ICF ^
   kernel32.lib ole32.lib oleaut32.lib runtimeobject.lib ws2_32.lib bcrypt.lib
set ERR=%ERRORLEVEL%
popd
if %ERR% neq 0 (echo BUILD FAILED & exit /b %ERR%)
echo Built %ROOT%build\Microsoft.Xbox.Services.dll
