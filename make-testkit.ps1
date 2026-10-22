# Builds a tester kit (mod only, no game files) from testkit-stage<b|c>\ plus a pdnet build:
#   D:\PhantomDust-pdnet-Stage<B|C>-TestKit-<DLL MD5 prefix>[-with-eos].zip
# The name carries the DLL's fingerprint (not the date: this PC's clock jumps) and says whether eos.ini is inside.
#   .\make-testkit.ps1 -Stage C -EosIni .\eos.tester.ini     stage C kit (EOS P2P), credentials inside
#   .\make-testkit.ps1 -Stage B                              stage B kit (direct UDP), no eos.ini (send it separately)
#   .\make-testkit.ps1 -EosIni .\eos.tester.ini              the kit (default, -Stage Kit): Store install AND unpacked copies, no game
#                                                            files changed (pdlaunch, testkit-store\) -> D:\PhantomDust-pdnet Experimental <N>.zip
#   -Stage C / B: the older kits that replace Microsoft.Xbox.Services.dll in an unpacked copy (testkit-stagec\ / b\)
#   -Build <folder under pdnet>   default build-discord4
# eos.ini holds an EOS client id and secret. Use the tester client (eos.tester.ini), not the development client
# in _netmod\eos.local.ini (see HANDOFF §9).
#   -Webhook <Discord webhook URL>   stage C: players' games announce the lobbies they host there
#            (default: webhook_url from pdnet\tools\out\lobbywatch.ini; -Webhook none = no Discord messages)
#
# Kit names (-Stage Kit), as the project owner asked (2026-10-22): every new kit is the next
#   D:\PhantomDust-pdnet Experimental <N>.zip
# and a kit becomes D:\PhantomDust-pdnet Stable <M>.zip only when the owner says it is stable:
#   .\make-testkit.ps1 -Promote <N>        renames Experimental N to the next Stable number
# Each kit carries kit-version.txt (label, pdnet.dll MD5, build, -Notes).
#   -Set key=value,...   pdnet.ini overrides for this kit (e.g. -Set voice_chat=off,p2p_debug=on)
#   -Notes "..."         one line for kit-version.txt (what changed, what to test)
param([ValidateSet('B', 'C', 'Store', 'Kit')][string]$Stage = 'Kit', [string]$EosIni, [string]$Build, [string]$Webhook,
      [string[]]$Set, [string]$Notes, [int]$Promote)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

# The highest "<kind> N" number among D:\PhantomDust-pdnet <kind> N.zip.
function LastNumber([string]$kind) {
    $numbers = Get-ChildItem 'D:\' -Filter "PhantomDust-pdnet $kind *.zip" | ForEach-Object { if ($_.BaseName -match " $kind (\d+)$") { [int]$Matches[1] } }
    if ($numbers) { ($numbers | Measure-Object -Maximum).Maximum } else { 0 }
}

# Rewrites kit-version.txt inside a kit zip: first line the label, the rest kept.
function SetZipLabel([string]$zip, [string]$label) {
    $z = [IO.Compression.ZipFile]::Open($zip, 'Update')
    try {
        $entry = $z.GetEntry('kit-version.txt')
        $rest = ''
        if ($entry) {
            $reader = New-Object IO.StreamReader($entry.Open())
            $lines = $reader.ReadToEnd() -split "`r?`n"
            $reader.Dispose()
            $rest = ($lines | Select-Object -Skip 1) -join "`r`n"
            $entry.Delete()
        }
        $writer = New-Object IO.StreamWriter($z.CreateEntry('kit-version.txt').Open(), (New-Object Text.UTF8Encoding $false))
        $writer.Write("$label`r`n$rest")
        $writer.Dispose()
    }
    finally { $z.Dispose() }
}

if ($Promote) {
    $from = "D:\PhantomDust-pdnet Experimental $Promote.zip"
    if (-not (Test-Path $from)) { throw "No $from." }
    $label = "PhantomDust-pdnet Stable $((LastNumber 'Stable') + 1)"
    Rename-Item $from "$label.zip"
    SetZipLabel "D:\$label.zip" $label
    Write-Host "Experimental $Promote is now D:\$label.zip"
    exit 0
}
if (-not $Build) { $Build = 'build-discord4' }
$launcherKit = $Stage -in 'Store', 'Kit'
$kitSource = if ($launcherKit) { "$root\testkit-store" } else { "$root\testkit-stage$($Stage.ToLower())" }

$dll = "$root\pdnet\$Build\Microsoft.Xbox.Services.dll"
if (-not (Test-Path $dll)) { throw "No DLL at $dll. Build it first: pdnet\build.cmd $Build" }
$md5 = (Get-FileHash $dll -Algorithm MD5).Hash
# Experimental numbers never repeat, even after one was promoted to Stable: the last one used is kept in kit-counter.txt.
$counterFile = "$root\kit-counter.txt"
$experimental = [Math]::Max((LastNumber 'Experimental'), $(if (Test-Path $counterFile) { [int](Get-Content $counterFile -Raw).Trim() } else { 0 })) + 1
$name = if ($Stage -eq 'Kit') { "PhantomDust-pdnet Experimental $experimental" }
        else { "PhantomDust-pdnet-$(if ($Stage -eq 'Store') { 'StageC-Store-TestKit' } else { "Stage$Stage-TestKit" })-$($md5.Substring(0, 8))" + $(if ($EosIni) { '-with-eos' } else { '' }) }
$out = "D:\$name"
$zip = "D:\$name.zip"
$sdkDll = Get-ChildItem 'C:\Users\ryhol\tools\downloads\eos-sdk' -Recurse -Filter 'EOSSDK-Win64-Shipping.dll' | Select-Object -First 1
if (-not $sdkDll) { throw 'EOSSDK-Win64-Shipping.dll not found under C:\Users\ryhol\tools\downloads\eos-sdk.' }
if ($EosIni) {
    if (-not (Test-Path $EosIni)) { throw "No file at $EosIni." }
    if ((Resolve-Path $EosIni).Path -eq (Resolve-Path "$root\eos.local.ini" -ErrorAction SilentlyContinue).Path) {
        Write-Host 'Warning: that is the development client (eos.local.ini). A tester client is safer to share.' -ForegroundColor Yellow
        if ((Read-Host 'Include it anyway? (y/n)') -ne 'y') { exit 1 }
    }
}

if (Test-Path $out) { Remove-Item $out -Recurse -Force }
if ($launcherKit) {
    # pdnet.dll + pdlaunch.exe next to the scripts; settings are the stage C ones.
    $launcher = "$root\pdnet\tools\out\pdlaunch.exe"
    if (-not (Test-Path $launcher)) { throw "No $launcher. Build it first: pdnet\tools\build-pdlaunch.cmd" }
    New-Item -ItemType Directory $out | Out-Null
    Copy-Item "$kitSource\*" $out
    Copy-Item "$root\testkit-stagec\pdnet.ini" $out
    Copy-Item $dll "$out\pdnet.dll"
    Copy-Item $launcher $out
} else {
    New-Item -ItemType Directory "$out\pdnet" | Out-Null
    Copy-Item "$kitSource\*" $out
    Copy-Item $dll "$out\pdnet\"
}
Copy-Item $sdkDll.FullName $out
# Voice chat (voice_chat=on): EOS plays and records through XAudio 2.9, shipped with the SDK.
Copy-Item (Join-Path $sdkDll.DirectoryName 'x64\xaudio2_9redist.dll') $out
if ($EosIni) { Copy-Item $EosIni "$out\eos.ini" }
if ($Stage -ne 'B') {
    $watchIni = "$root\pdnet\tools\out\lobbywatch.ini"
    if (-not $Webhook -and (Test-Path $watchIni)) {
        $line = Select-String -Path $watchIni -Pattern '^webhook_url=(.+)$' | Select-Object -First 1
        if ($line) { $Webhook = $line.Matches[0].Groups[1].Value.Trim() }
    }
    if ($Webhook -eq 'none') { $Webhook = '' }
    $ini = (Get-Content "$out\pdnet.ini" -Raw -Encoding UTF8) -replace '(?m)^discord_webhook=.*$', "discord_webhook=$Webhook"
    foreach ($pair in $Set) {
        $key, $value = $pair -split '=', 2
        if ($ini -match "(?m)^$([regex]::Escape($key))=") { $ini = $ini -replace "(?m)^$([regex]::Escape($key))=.*$", "$key=$value" }
        else { $ini = $ini.TrimEnd() + "`r`n$key=$value`r`n" }
    }
    Set-Content "$out\pdnet.ini" $ini -Encoding UTF8 -NoNewline
}
if ($Stage -eq 'Kit') {
    $version = "$name`r`npdnet.dll MD5 $($md5.Substring(0, 8)) ($Build)`r`n" + $(if ($Set) { "Settings: $($Set -join ', ')`r`n" } else { '' }) + $(if ($Notes) { "$Notes`r`n" } else { '' })
    [IO.File]::WriteAllText("$out\kit-version.txt", $version, (New-Object Text.UTF8Encoding $false))
}

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$out\*" -DestinationPath $zip
if ($Stage -eq 'Kit') { Set-Content $counterFile $experimental }
# Only the zip is for handing out: an unzipped copy here gets used by mistake (Setup.bat run from it points the game
# at a folder this script deletes and rebuilds).
Remove-Item $out -Recurse -Force
Write-Host "Kit: $zip  (stage $Stage, pdnet DLL MD5 $md5 from $Build)"
if ($Stage -ne 'B') {
    if ($Webhook) { Write-Host 'The Discord webhook URL is INSIDE the zip (pdnet.ini): anyone with the kit can post to that channel.' -ForegroundColor Yellow }
    else { Write-Host 'No Discord webhook in this kit (lobbies are not announced).' }
}
if ($EosIni) { Write-Host 'eos.ini is INSIDE the zip: share it only with your testers.' -ForegroundColor Yellow }
else { Write-Host 'eos.ini is NOT in the zip: send each tester an eos.ini separately (they put it next to install.ps1).' -ForegroundColor Yellow }
