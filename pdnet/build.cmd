@echo off
rem Builds pdnet as build\Microsoft.Xbox.Services.dll (x64, static CRT so it needs nothing from the package).
rem Optional argument: a different output folder name, e.g. "build.cmd build-test".
setlocal
set ROOT=%~dp0
set OUT=build
if not "%~1"=="" set OUT=%~1
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (
    echo Visual Studio C++ tools not found. Install the "Desktop development with C++" workload.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.22000.0 >nul || exit /b 1

rem EOS SDK headers (only needed for the eos_test feasibility check; the SDK DLL is loaded at run time).
if not defined EOS_SDK set EOS_SDK=C:\Users\ryhol\tools\downloads\eos-sdk\EOS-SDK-58105819-Release-v1.19.2.1\SDK
set EOSFLAGS=/DPDNET_NO_EOS
if exist "%EOS_SDK%\Include\eos_sdk.h" set EOSFLAGS=/I"%EOS_SDK%\Include"

if not exist "%ROOT%%OUT%" mkdir "%ROOT%%OUT%"
pushd "%ROOT%%OUT%"
cl /nologo /std:c++20 /EHsc /MT /O2 /Zi /W3 /permissive- /bigobj /utf-8 ^
   /DUNICODE /D_UNICODE %EOSFLAGS% ^
   "%ROOT%src\dllmain.cpp" "%ROOT%src\hooks.cpp" "%ROOT%src\gamesave.cpp" "%ROOT%src\log.cpp" ^
   "%ROOT%src\config.cpp" "%ROOT%src\net.cpp" "%ROOT%src\sda.cpp" "%ROOT%src\testpeer.cpp" "%ROOT%src\eostest.cpp" ^
   /LD /Fe:Microsoft.Xbox.Services.dll ^
   /link /DEF:"%ROOT%src\pdnet.def" /DEBUG /OPT:REF /OPT:ICF ^
   kernel32.lib ole32.lib oleaut32.lib runtimeobject.lib ws2_32.lib bcrypt.lib
set ERR=%ERRORLEVEL%
popd
if %ERR% neq 0 (echo BUILD FAILED & exit /b %ERR%)
echo Built %ROOT%%OUT%\Microsoft.Xbox.Services.dll
