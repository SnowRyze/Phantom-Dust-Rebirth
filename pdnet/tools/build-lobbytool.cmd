@echo off
rem Builds tools\out\lobbytool.exe (a second EOS client for testing stage B on one PC) and copies the EOS SDK DLL
rem and _netmod\eos.local.ini (as eos.ini) next to it. tools\out is git-ignored; never commit eos.ini.
setlocal
set ROOT=%~dp0..\
set OUT=%~dp0out
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.22000.0 >nul || exit /b 1
if not defined EOS_SDK set EOS_SDK=C:\Users\ryhol\tools\downloads\eos-sdk\EOS-SDK-58105819-Release-v1.19.2.1\SDK

if not exist "%OUT%\obj" mkdir "%OUT%\obj"
pushd "%OUT%"
cl /nologo /std:c++20 /EHsc /MT /O2 /Zi /W3 /permissive- /utf-8 /DUNICODE /D_UNICODE /MP /I"%ROOT%src" /I"%EOS_SDK%\Include" ^
   /Fo"obj\\" /Fd"obj\lobbytool.pdb" /Fe"lobbytool.exe" ^
   "%~dp0lobbytool.cpp" "%ROOT%src\lobbies.cpp" "%ROOT%src\eos.cpp" "%ROOT%src\eossdk.cpp" "%ROOT%src\net.cpp" "%ROOT%src\p2p.cpp" "%ROOT%src\discord.cpp" ^
   /link /DEBUG kernel32.lib ole32.lib oleaut32.lib runtimeobject.lib advapi32.lib ws2_32.lib bcrypt.lib winmm.lib || goto :failed
copy /y "%EOS_SDK%\Bin\EOSSDK-Win64-Shipping.dll" . >nul || goto :failed
copy /y "%EOS_SDK%\Bin\x64\xaudio2_9redist.dll" . >nul || goto :failed
copy /y "%ROOT%..\eos.local.ini" eos.ini >nul || goto :failed
popd
echo Built %OUT%\lobbytool.exe
exit /b 0

:failed
popd
echo BUILD FAILED
exit /b 1
