# The one build script (started by build.bat). Everything it makes goes in out\:
#   out\pdnet.dll, out\pdlaunch.exe   the mod (pdnet.dll is the game's Microsoft.Xbox.Services.dll stand-in) and
#                                     its launcher
#   out\kit\                          a ready-to-use kit: Setup.bat + pdnet\ (what testers get)
#   out\PhantomDust-pdnet.zip         that kit as a zip
# Arguments:
#   (none)                    build all of the above
#   experimental "notes"      also save the kit as the next "PhantomDust-pdnet Experimental <N>.zip" in the kits
#                             folder (D:\ if it exists, else out\kits; or set PDNET_KITS), with the notes in
#                             pdnet\kit-version.txt
#   promote <N>               rename Experimental N to the next "Stable <M>" and tag its source commit stable-<M>
# Needs: Visual Studio 2022 (or Build Tools) with "Desktop development with C++", and Epic's EOS SDK in deps\eos-sdk.
# Optional, in private\ (never in git): eos.tester.ini (the EOS client testers use, shipped as the kit's eos.ini) and
# links.ini (discord_webhook=, log_webhook=, discord_invite=, put into the kit's pdnet.ini).
param([Parameter(Position = 0)][string]$Mode = 'build', [Parameter(Position = 1)][string]$Arg)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'out'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

function Step([string]$text) { Write-Host "`n== $text" -ForegroundColor Cyan }
function Fail([string]$text) { Write-Host "`n$text" -ForegroundColor Red; exit 1 }

$kits = if ($env:PDNET_KITS) { $env:PDNET_KITS } elseif (Test-Path 'D:\') { 'D:\' } else { Join-Path $out 'kits' }
$counterFile = Join-Path $PSScriptRoot 'kit-counter.txt'

# The highest "<kind> N" among "<kits>\PhantomDust-pdnet <kind> N.zip".
function LastNumber([string]$kind) {
    if (-not (Test-Path $kits)) { return 0 }
    $numbers = Get-ChildItem $kits -Filter "PhantomDust-pdnet $kind *.zip" | ForEach-Object { if ($_.BaseName -match " $kind (\d+)$") { [int]$Matches[1] } }
    if ($numbers) { ($numbers | Measure-Object -Maximum).Maximum } else { 0 }
}

# pdnet/kit-version.txt in a kit zip (kits from before the pdnet\ folder had it at the top).
function VersionEntry($zip) { $e = $zip.GetEntry('pdnet/kit-version.txt'); if ($e) { $e } else { $zip.GetEntry('kit-version.txt') } }

# A folder as a zip, entry by entry with "pdnet/x" names (Windows PowerShell's Compress-Archive writes "pdnet\x",
# which some unzip tools and the kit's own scripts don't read as a folder).
function Zip([string]$folder, [string]$zipPath) {
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    $archive = [IO.Compression.ZipFile]::Open($zipPath, 'Create')
    try {
        foreach ($file in Get-ChildItem $folder -Recurse -File) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $file.FullName.Substring($folder.Length + 1).Replace('\', '/')) | Out-Null
        }
    }
    finally { $archive.Dispose() }
}

# ---- promote: only renames and tags, no build ----------------------------------------------------------------------
if ($Mode -eq 'promote') {
    $n = [int]$Arg
    $from = Join-Path $kits "PhantomDust-pdnet Experimental $n.zip"
    if (-not (Test-Path $from)) { Fail "No $from." }
    $number = (LastNumber 'Stable') + 1
    $label = "PhantomDust-pdnet Stable $number"
    $to = Join-Path $kits "$label.zip"
    Rename-Item $from "$label.zip"
    $z = [IO.Compression.ZipFile]::Open($to, 'Update')
    try {
        $entry = VersionEntry $z
        $path = if ($entry) { $entry.FullName } else { 'pdnet/kit-version.txt' }
        $lines = @()
        if ($entry) { $r = New-Object IO.StreamReader($entry.Open()); $lines = $r.ReadToEnd() -split "`r?`n"; $r.Dispose(); $entry.Delete() }
        $source = $lines | Where-Object { $_ -match '^Source: (\w+)' } | ForEach-Object { $Matches[1] } | Select-Object -First 1
        $w = New-Object IO.StreamWriter($z.CreateEntry($path).Open(), (New-Object Text.UTF8Encoding $false))
        $w.Write("$label`r`n" + (($lines | Select-Object -Skip 1) -join "`r`n")); $w.Dispose()
    }
    finally { $z.Dispose() }
    if ($source) { git -C $root tag -f "stable-$number" $source | Out-Null; Write-Host "git tag stable-$number -> $source (push it: git push origin stable-$number)" }
    else { Write-Host "No Source line in kit-version.txt: tag stable-$number by hand." -ForegroundColor Yellow }
    Write-Host "Experimental $n is now $to"
    exit 0
}
if ($Mode -notin 'build', 'experimental') { Fail "Unknown argument '$Mode'. Use: build.bat [experimental `"notes`" | promote N]" }

# ---- tools ---------------------------------------------------------------------------------------------------------
Step 'Looking for Visual Studio and Epic''s SDK'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = if (Test-Path $vswhere) { & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath } else { $null }
if (-not $vs) {
    Fail ("Visual Studio's C++ compiler was not found.`n" +
          "Install Visual Studio 2022 Community (or 'Build Tools for Visual Studio 2022') from https://visualstudio.microsoft.com/downloads/`n" +
          "and tick the workload 'Desktop development with C++'. Then run build.bat again.")
}
# The SDK the code was written against, when it is there; else Visual Studio's default.
$winSdk = if (Test-Path (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include\10.0.22000.0\cppwinrt')) { '10.0.22000.0' } else { '' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
foreach ($line in (cmd /c "`"$vcvars`" x64 $winSdk >nul 2>&1 && set")) { if ($line -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { Fail "Visual Studio was found at $vs, but its C++ compiler didn't start. Repair the 'Desktop development with C++' workload." }
Write-Host "Visual Studio: $vs$(if ($winSdk) { " (Windows SDK $winSdk)" })"

$eosHeader = Get-ChildItem (Join-Path $root 'deps\eos-sdk') -Recurse -Filter 'eos_sdk.h' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($env:EOS_SDK -and (Test-Path (Join-Path $env:EOS_SDK 'Include\eos_sdk.h'))) { $eosHeader = Get-Item (Join-Path $env:EOS_SDK 'Include\eos_sdk.h') }
if (-not $eosHeader) {
    Fail ("Epic's EOS SDK was not found.`n" +
          "1. Sign in at https://dev.epicgames.com/portal, open SDK & Release Notes, and download the C SDK for Windows (EOS-SDK-...-Release-v1.19.x).`n" +
          "2. Unzip it into deps\eos-sdk (so that deps\eos-sdk\...\Include\eos_sdk.h exists).`n" +
          "   Its license doesn't allow it in the repository, so everyone downloads it once.`n" +
          "Then run build.bat again.")
}
$eosSdk = Split-Path (Split-Path $eosHeader.FullName -Parent) -Parent
$eosDll = Join-Path $eosSdk 'Bin\EOSSDK-Win64-Shipping.dll'
$xaudio = Join-Path $eosSdk 'Bin\x64\xaudio2_9redist.dll'
foreach ($f in $eosDll, $xaudio) { if (-not (Test-Path $f)) { Fail "Epic's SDK at $eosSdk is missing $f (a different SDK layout?)." } }
Write-Host "Epic's SDK: $eosSdk"

function Run([string]$what, [string[]]$arguments) {
    & cl.exe @arguments
    if ($LASTEXITCODE -ne 0) { Fail "$what failed to compile (see the errors above)." }
}

# ---- pdnet.dll -----------------------------------------------------------------------------------------------------
Step 'Building pdnet.dll'
$obj = Join-Path $out 'obj'
if (Test-Path $obj) { Remove-Item $obj -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $obj 'xsapi'), (Join-Path $obj 'launcher') | Out-Null
$src = Join-Path $root 'src'
$xsapi = Join-Path $root 'xsapi'
$common = @('/nologo', '/c', '/std:c++20', '/EHsc', '/MT', '/O2', '/Zi', '/W3', '/permissive-', '/bigobj', '/utf-8', '/DUNICODE', '/D_UNICODE', '/FS')
Push-Location $out
try {
    # 1. pdnet's own code, 2. the Xbox Live stand-in (precompiled header, then every class), 3. link.
    # Output paths are relative to out\ (an absolute one with a space would end in \" and confuse cl's parsing).
    Run 'pdnet' ($common + @('/MP', "/I$(Split-Path $eosHeader.FullName -Parent)", '/Foobj\', '/Fdobj\core.pdb') + (Get-ChildItem $src -Filter *.cpp).FullName)
    $xinc = @("/I$xsapi", "/I$xsapi\gen", "/I$xsapi\stubs")
    Run 'The Xbox Live stand-in' ($common + $xinc + @('/Ycxsapi_pch.h', '/Fpobj\xsapi\xsapi.pch', '/Foobj\xsapi\', '/Fdobj\xsapi.pdb', "$xsapi\xsapi_pch.cpp"))
    # The ~330 stub classes as a wildcard cl expands itself: listed one by one they overflow the command line of its
    # parallel workers (error D8040).
    $classes = @("$xsapi\xsapi.cpp", "$xsapi\mpsd.cpp", "$xsapi\nametrace.cpp", "$xsapi\gen\module.g.cpp", "$xsapi\stubs\*.cpp")
    Run 'The Xbox Live stand-in' ($common + $xinc + @('/MP', '/Yuxsapi_pch.h', '/Fpobj\xsapi\xsapi.pch', '/Foobj\xsapi\', '/Fdobj\xsapi.pdb') + $classes)
    & link.exe /nologo /DLL /OUT:pdnet.dll "/DEF:$src\pdnet.def" /DEBUG /OPT:REF /OPT:ICF /IGNORE:4070 /NOIMPLIB /NOEXP 'obj\*.obj' 'obj\xsapi\*.obj' kernel32.lib ole32.lib oleaut32.lib runtimeobject.lib ws2_32.lib bcrypt.lib
    if ($LASTEXITCODE -ne 0) { Fail 'pdnet.dll failed to link (see the errors above).' }

    # ---- pdlaunch.exe: a Windows program, not a console one (it stays running in the background with the game) ----
    Step 'Building pdlaunch.exe'
    $sdl = Join-Path $root 'deps\SDL3'
    & cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /permissive- /utf-8 /DUNICODE /D_UNICODE "/I$src" "/I$sdl\include" '/Foobj\launcher\' /Fepdlaunch.exe "$root\launcher\pdlaunch.cpp" /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup kernel32.lib psapi.lib ole32.lib user32.lib
    if ($LASTEXITCODE -ne 0) { Fail 'pdlaunch.exe failed to build (see the errors above).' }
    Copy-Item "$sdl\SDL3.dll" $out -Force
}
finally { Pop-Location }
Remove-Item $obj -Recurse -Force # about 1 GB of intermediate files

# ---- the kit -------------------------------------------------------------------------------------------------------
Step 'Making the kit'
$kit = Join-Path $out 'kit'
if (Test-Path $kit) { Remove-Item $kit -Recurse -Force }
Copy-Item (Join-Path $root 'kit') $kit -Recurse
$files = Join-Path $kit 'pdnet'
Copy-Item (Join-Path $out 'pdnet.dll'), (Join-Path $out 'pdlaunch.exe'), (Join-Path $out 'SDL3.dll'), $eosDll, $xaudio $files

$eosIni = Join-Path $root 'private\eos.tester.ini'
if (Test-Path $eosIni) { Copy-Item $eosIni (Join-Path $files 'eos.ini') }
else { Write-Host "No private\eos.tester.ini: the kit has no eos.ini, so its players can't sign in to Epic (online play won't work)." -ForegroundColor Yellow }

$ini = Get-Content (Join-Path $files 'pdnet.ini') -Raw -Encoding UTF8
$links = Join-Path $root 'private\links.ini'
foreach ($key in 'discord_webhook', 'log_webhook', 'discord_invite') {
    $value = if (Test-Path $links) { Select-String -Path $links -Pattern "^$key=(.+)$" | ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() } | Select-Object -First 1 } else { '' }
    $ini = $ini -replace "(?m)^$key=.*$", "$key=$value"
    if (-not $value) { Write-Host "No $key in private\links.ini: left empty in the kit." -ForegroundColor Yellow }
}
Set-Content (Join-Path $files 'pdnet.ini') $ini -Encoding UTF8 -NoNewline

$md5 = (Get-FileHash (Join-Path $out 'pdnet.dll') -Algorithm MD5).Hash.Substring(0, 8)
$commit = git -C $root rev-parse --short HEAD 2>$null
$dirty = if (git -C $root status --porcelain -- src xsapi launcher kit 2>$null) { ' + uncommitted changes' } else { '' }
$label = if ($Mode -eq 'experimental') {
    $number = [Math]::Max((LastNumber 'Experimental'), $(if (Test-Path $counterFile) { [int](Get-Content $counterFile -Raw).Trim() } else { 0 })) + 1
    "PhantomDust-pdnet Experimental $number"
} else { 'PhantomDust-pdnet development build' }
$version = "$label`r`npdnet.dll MD5 $md5`r`nSource: $(if ($commit) { $commit } else { 'unknown' })$dirty`r`n" + $(if ($Mode -eq 'experimental' -and $Arg) { "$Arg`r`n" } else { '' })
[IO.File]::WriteAllText((Join-Path $files 'kit-version.txt'), $version, (New-Object Text.UTF8Encoding $false))
Zip $kit (Join-Path $out 'PhantomDust-pdnet.zip')

Write-Host "`nBuilt:" -ForegroundColor Green
Write-Host "  $out\pdnet.dll, pdlaunch.exe"
Write-Host "  $kit  (unzipped kit: run its Setup.bat to install on this PC)"
Write-Host "  $out\PhantomDust-pdnet.zip"

if ($Mode -eq 'experimental') {
    if ($dirty) { Write-Host 'Note: there are uncommitted changes; the kit says so in kit-version.txt.' -ForegroundColor Yellow }
    New-Item -ItemType Directory -Force $kits | Out-Null
    $zip = Join-Path $kits "$label.zip"
    Copy-Item (Join-Path $out 'PhantomDust-pdnet.zip') $zip -Force
    Set-Content $counterFile $number
    Write-Host "  $zip" -ForegroundColor Green
    Write-Host 'The kit has eos.ini and the Discord links inside: share it only with your testers.' -ForegroundColor Yellow
}
