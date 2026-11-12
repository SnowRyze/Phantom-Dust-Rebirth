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
#   promote <N>               make Experimental N the next "Stable <M>" (the public version) with
#                             private\public-links.ini, tag stable-<M> and publish it (the whole kit for first installs,
#                             plus pdnet-update-<build>.zip for updates)
#   conceptual "notes"        save the kit as the next "PhantomDust-pdnet Conceptual <N>.zip" (from the Conceptual
#                             branch): never published, and its pdlaunch never updates it
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

# ---- updates: GitHub Releases ----------------------------------------------------------------------------------------
# Players' pdlaunch updates itself from this repository's releases (launcher\pdlaunch.cpp, CheckForUpdate): each
# Experimental is a pre-release "build-<N>", each Stable a release "stable-<M>", with the asset pdnet-update-<N>.zip
# (the kit without eos.ini and without the Discord links) and the line "sha256 pdnet-update-<N>.zip <hex>" in its
# notes. <N> is the build number: the Experimental number, which a Stable keeps. Publishing needs a GitHub token in
# private\github-token.txt (a fine-grained token for this repository with "Contents: Read and write").

# The kit folder `from` as a secret-free update zip at `to`.
function UpdateZip([string]$from, [string]$to) {
    $tmp = Join-Path $out 'update-kit'
    if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
    Copy-Item $from $tmp -Recurse
    Remove-Item (Join-Path $tmp 'pdnet\eos.ini') -ErrorAction SilentlyContinue
    $iniPath = Join-Path $tmp 'pdnet\pdnet.ini'
    $text = Get-Content $iniPath -Raw -Encoding UTF8
    foreach ($key in 'discord_webhook', 'log_webhook', 'discord_invite') { $text = $text -replace "(?m)^$key=.*$", "$key=" }
    Set-Content $iniPath $text -Encoding UTF8 -NoNewline
    Zip $tmp $to
    Remove-Item $tmp -Recurse -Force
}

# owner/name of the GitHub repository (from the git remote; this one if there is none).
function RepoSlug {
    $url = $null
    if (Get-Command git -ErrorAction SilentlyContinue) { $ErrorActionPreference = 'Continue'; $url = git -C $root remote get-url origin 2>$null; $ErrorActionPreference = 'Stop' }
    if ($url -match 'github\.com[:/](.+?)(\.git)?$') { $Matches[1] } else { 'SnowRyze/Phantom-Dust-Rebirth' }
}

# Creates a GitHub release with its assets; returns its web page, or $null (with the reason printed) if it didn't happen.
# $labels maps an asset's file name to the name GitHub shows for it (the file name, which the updater looks for, stays).
function PublishRelease([string]$tag, [string]$target, [string]$title, [bool]$prerelease, [string]$notes, [string[]]$assetPaths, [hashtable]$labels = @{}) {
    $tokenFile = Join-Path $root 'private\github-token.txt'
    if (-not (Test-Path $tokenFile)) { Write-Host "Not published to GitHub: no private\github-token.txt (see README, Publishing updates)." -ForegroundColor Yellow; return $null }
    $headers = @{ Authorization = "Bearer $((Get-Content $tokenFile -Raw).Trim())"; Accept = 'application/vnd.github+json'; 'User-Agent' = 'pdnet-build' }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $repo = RepoSlug
    try {
        $body = @{ tag_name = $tag; name = $title; body = $notes; prerelease = $prerelease }
        if ($target) { $body.target_commitish = $target }
        $release = Invoke-RestMethod -Method Post -Uri "https://api.github.com/repos/$repo/releases" -Headers $headers -Body ($body | ConvertTo-Json) -ContentType 'application/json'
        foreach ($assetPath in $assetPaths) {
            $leaf = Split-Path $assetPath -Leaf
            $upload = ($release.upload_url -replace '\{.*$', '') + '?name=' + [Uri]::EscapeDataString($leaf)
            if ($labels.ContainsKey($leaf)) { $upload += '&label=' + [Uri]::EscapeDataString($labels[$leaf]) }
            Invoke-RestMethod -Method Post -Uri $upload -Headers $headers -InFile $assetPath -ContentType 'application/zip' | Out-Null
        }
        return $release.html_url
    }
    catch {
        $detail = $_.ErrorDetails.Message; if (-not $detail) { $detail = $_.Exception.Message }
        Write-Host "Not published to GitHub ($repo): $detail" -ForegroundColor Yellow
        return $null
    }
}

function ReleaseNotes([int]$build, [string]$assetPath, [string]$text) {
    $sha = (Get-FileHash $assetPath -Algorithm SHA256).Hash.ToLower()
    "Build $build`r`n`r`n$text`r`n`r`nsha256 $(Split-Path $assetPath -Leaf) $sha`r`n`r`nInstalled kits update themselves at game start (Main menu > Options > Updates). This download has no eos.ini and no Discord links: a first install needs the kit from the newest Stable release."
}

# ---- promote: Experimental N as the next "Stable <M>", the public version --------------------------------------------
# Stable is the version for everyone since 2026-11-10 (the Public kind is gone). The kit is the Experimental's, with
# kit-version.txt saying Stable <M> and the Discord links from private\public-links.ini (same keys as links.ini).
# Published as the full release "stable-<M>" with two files: the whole kit (eos.ini and the links inside, for first
# installs) and pdnet-update-<build>.zip, the secret-free update that the stable channel reads. No build.
if ($Mode -eq 'promote') {
    $n = [int]$Arg
    $from = Join-Path $kits "PhantomDust-pdnet Experimental $n.zip"
    if (-not (Test-Path $from)) { Fail "No $from." }
    $publicLinks = Join-Path $root 'private\public-links.ini'
    if (-not (Test-Path $publicLinks)) { Fail "No private\public-links.ini (discord_webhook=, log_webhook=, discord_invite= for Stable kits)." }
    $number = (LastNumber 'Stable') + 1
    $label = "PhantomDust-pdnet Stable $number"
    $to = Join-Path $kits "$label.zip"
    $unpacked = Join-Path $out 'promote-kit'
    if (Test-Path $unpacked) { Remove-Item $unpacked -Recurse -Force }
    [IO.Compression.ZipFile]::ExtractToDirectory($from, $unpacked)
    $versionPath = Join-Path $unpacked 'pdnet\kit-version.txt'
    $lines = (Get-Content $versionPath -Raw -Encoding UTF8) -split "`r?`n"
    $source = $lines | Where-Object { $_ -match '^Source: (\w+)' } | ForEach-Object { $Matches[1] } | Select-Object -First 1
    $build = $lines | Where-Object { $_ -match '^Build: (\d+)' } | ForEach-Object { [int]$Matches[1] } | Select-Object -First 1
    if (-not $build) { $build = $n } # kits from before the Build line: the build number is the Experimental number
    [IO.File]::WriteAllText($versionPath, "$label`r`n" + (($lines | Select-Object -Skip 1) -join "`r`n"), (New-Object Text.UTF8Encoding $false))
    $iniPath = Join-Path $unpacked 'pdnet\pdnet.ini'
    $ini = Get-Content $iniPath -Raw -Encoding UTF8
    foreach ($key in 'discord_webhook', 'log_webhook', 'discord_invite') {
        $value = Select-String -Path $publicLinks -Pattern "^$key=(.+)$" | ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() } | Select-Object -First 1
        if (-not $value) { Write-Host "No $key in private\public-links.ini: left empty in the Stable kit." -ForegroundColor Yellow }
        $ini = $ini -replace "(?m)^$key=.*$", "$key=$value"
    }
    Set-Content $iniPath $ini -Encoding UTF8 -NoNewline
    Zip $unpacked $to
    Remove-Item $from -Force
    Write-Host "Experimental $n is now $to"
    if ($source) { git -C $root tag -f "stable-$number" $source | Out-Null; Write-Host "git tag stable-$number -> $source (push it: git push origin stable-$number)" }
    else { Write-Host "No Source line in kit-version.txt: tag stable-$number by hand." -ForegroundColor Yellow }

    $asset = Join-Path $out "pdnet-update-$build.zip"
    UpdateZip $unpacked $asset
    Remove-Item $unpacked -Recurse -Force
    $download = Join-Path $out "PhantomDust-Rebirth-Stable-$number-PC.zip"
    Copy-Item $to $download -Force
    $fullSource = $null
    if ($source) { $ErrorActionPreference = 'Continue'; $fullSource = git -C $root rev-parse $source 2>$null; $ErrorActionPreference = 'Stop' } # GitHub wants the full hash
    $sha = (Get-FileHash $asset -Algorithm SHA256).Hash.ToLower()
    $notes = "Build $build`r`n`r`nPhantom Dust Rebirth, Stable $number (Experimental $n).`r`n`r`nFirst install: download $(Split-Path $download -Leaf), unzip it to a folder that stays put and double-click Setup.bat (see pdnet\README.txt).`r`n`r`nsha256 $(Split-Path $asset -Leaf) $sha`r`n`r`nInstalled kits update themselves at game start (Main menu > Options > Updates)."
    # The update zip has no eos.ini, so it can't be a first install: its title on the release page says so (the user,
    # 2026-11-10). Only the shown name changes; pdlaunch still finds it by its file name.
    $updateLabel = @{ (Split-Path $asset -Leaf) = "DO NOT DOWNLOAD - automatic updates only (pdnet-update-$build.zip)" }
    $page = PublishRelease "stable-$number" $fullSource "Stable $number" $false $notes @($download, $asset) $updateLabel
    if ($page) { Write-Host "Published: $page" -ForegroundColor Green }
    exit 0
}
if ($Mode -notin 'build', 'experimental', 'conceptual') { Fail "Unknown argument '$Mode'. Use: build.bat [experimental `"notes`" | conceptual `"notes`" | promote N]" }

# ---- tools ---------------------------------------------------------------------------------------------------------
Step 'Looking for Visual Studio and Epic''s SDK'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = if (Test-Path $vswhere) { & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath } else { $null }
if (-not $vs) {
    Fail ("Visual Studio's C++ compiler was not found.`n" +
          "Install Visual Studio 2022 Community (or 'Build Tools for Visual Studio 2022') from https://visualstudio.microsoft.com/downloads/`n" +
          "and tick the workload 'Desktop development with C++'. Then run build.bat again.")
}
# Windows SDK 10.0.22000.0 is required: the committed Xbox Live headers (xsapi\gen) were made by its C++/WinRT, and
# C++/WinRT refuses to mix versions ("Mismatched C++/WinRT headers", 2026-10-30 on a PC with only SDK 10.0.26100).
# Current Visual Studio 2022 (17.14) no longer offers it as a component (its installer answered "Cannot find package
# Microsoft.VisualStudio.Component.Windows11SDK.22000"), so this opens Microsoft's standalone installer for it in the
# browser: "Windows SDK for Windows 11 (10.0.22000.194)" on the Windows SDK archive page.
$sdkDir = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include\10.0.22000.0\cppwinrt'
$sdkDownload = 'https://go.microsoft.com/fwlink/?linkid=2173743'
$sdkArchive = 'https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads-archive'
if (-not (Test-Path $sdkDir)) {
    Write-Host "`nThe Windows 11 SDK 10.0.22000 is not installed. pdnet needs exactly that version: newer SDKs don't fit the" -ForegroundColor Yellow
    Write-Host "generated Xbox Live code, and Visual Studio's installer no longer offers it. Microsoft's own installer for it:" -ForegroundColor Yellow
    Write-Host "  $sdkDownload"
    Write-Host "  (on $sdkArchive : 'Windows SDK for Windows 11 (10.0.22000.194)', Installer)"
    $answer = Read-Host 'Open that download in your browser now? (Y/N)'
    if ($answer -match '^\s*y') { Start-Process $sdkDownload }
    Fail ("Run the downloaded winsdksetup.exe, keep its default selection, and let it install (Windows asks for permission).`n" +
          "It installs next to any newer SDK; nothing else changes. Then run build.bat again.")
}
$winSdk = '10.0.22000.0'
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
# The commit it was built from, when this is a git clone with git installed: a download of the repository as a ZIP
# has no .git folder, and in Windows PowerShell git's "not a git repository" would stop the build (2026-10-29).
$commit = $null; $dirty = ''
if ((Test-Path (Join-Path $root '.git')) -and (Get-Command git -ErrorAction SilentlyContinue)) {
    $ErrorActionPreference = 'Continue'
    $commit = git -C $root rev-parse --short HEAD 2>$null
    if (git -C $root status --porcelain -- src xsapi launcher kit 2>$null) { $dirty = ' + uncommitted changes' }
    $ErrorActionPreference = 'Stop'
}
$label = if ($Mode -eq 'experimental') {
    $number = [Math]::Max((LastNumber 'Experimental'), $(if (Test-Path $counterFile) { [int](Get-Content $counterFile -Raw).Trim() } else { 0 })) + 1
    "PhantomDust-pdnet Experimental $number"
} elseif ($Mode -eq 'conceptual') { "PhantomDust-pdnet Conceptual $((LastNumber 'Conceptual') + 1)" }
else { 'PhantomDust-pdnet development build' }
$version = "$label`r`npdnet.dll MD5 $md5`r`n" + $(if ($Mode -eq 'experimental') { "Build: $number`r`n" } else { '' }) +
           "Source: $(if ($commit) { $commit } else { 'unknown (not built from a git clone)' })$dirty`r`n" + $(if ($Mode -ne 'build' -and $Arg) { "$Arg`r`n" } else { '' })
[IO.File]::WriteAllText((Join-Path $files 'kit-version.txt'), $version, (New-Object Text.UTF8Encoding $false))
Zip $kit (Join-Path $out 'PhantomDust-pdnet.zip')

Write-Host "`nBuilt:" -ForegroundColor Green
Write-Host "  $out\pdnet.dll, pdlaunch.exe"
Write-Host "  $kit  (unzipped kit: run its Setup.bat to install on this PC)"
Write-Host "  $out\PhantomDust-pdnet.zip"

if ($Mode -eq 'conceptual') {
    if (-not (Test-Path $kits)) { New-Item -ItemType Directory -Force $kits | Out-Null }
    $zip = Join-Path $kits "$label.zip"
    Copy-Item (Join-Path $out 'PhantomDust-pdnet.zip') $zip -Force
    Write-Host "  $zip" -ForegroundColor Green
    Write-Host 'Conceptual: not published, and it never updates itself. The kit has eos.ini and the Discord links inside: share it only with your testers.' -ForegroundColor Yellow
}

if ($Mode -eq 'experimental') {
    if ($dirty) { Write-Host 'Note: there are uncommitted changes; the kit says so in kit-version.txt.' -ForegroundColor Yellow }
    if (-not (Test-Path $kits)) { New-Item -ItemType Directory -Force $kits | Out-Null }
    $zip = Join-Path $kits "$label.zip"
    Copy-Item (Join-Path $out 'PhantomDust-pdnet.zip') $zip -Force
    Set-Content $counterFile $number
    Write-Host "  $zip" -ForegroundColor Green
    Write-Host 'The kit has eos.ini and the Discord links inside: share it only with your testers.' -ForegroundColor Yellow

    # The update for installed kits on the experimental channel: a GitHub pre-release of the commit it was built from,
    # which has to be on GitHub already (push first).
    $asset = Join-Path $out "pdnet-update-$number.zip"
    UpdateZip $kit $asset
    $fullCommit = $null; $pushed = $false
    if ($commit) {
        $ErrorActionPreference = 'Continue'
        $fullCommit = git -C $root rev-parse HEAD 2>$null
        git -C $root fetch -q origin 2>$null
        $pushed = [bool](git -C $root branch -r --contains $fullCommit 2>$null)
        $ErrorActionPreference = 'Stop'
    }
    if (-not $fullCommit) { Write-Host 'Not published to GitHub: not a git clone.' -ForegroundColor Yellow }
    elseif ($dirty) { Write-Host 'Not published to GitHub: commit the changes first (an update must match a commit).' -ForegroundColor Yellow }
    elseif (-not $pushed) { Write-Host 'Not published to GitHub: push this commit first (git push), then run build.bat experimental again.' -ForegroundColor Yellow }
    else {
        $page = PublishRelease "build-$number" $fullCommit "Experimental $number" $true (ReleaseNotes $number $asset $(if ($Arg) { $Arg } else { "Experimental $number." })) $asset
        if ($page) { Write-Host "Published: $page" -ForegroundColor Green }
    }
}
