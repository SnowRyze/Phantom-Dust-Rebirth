# Runs the Direct build in Xbox-free mode (pdnet.ini: xbox=off): no Xbox Live sign-in, pdnet answers every Xbox Live
# class itself. The Teredo build is not touched.
#   .\xboxfree.ps1 [-Name "MyName"]   prepare D:\PhantomDust-Direct, register it, keep it running when minimized, launch
#   .\xboxfree.ps1 -Report            what the game asked pdnet for, which stand-ins it hit, and whether it crashed
#   .\xboxfree.ps1 -Off               put the Direct build back to normal Xbox Live mode (xbox=on)
# Afterwards switch back with:  .\builds.ps1 -Use Teredo
param([string]$Name, [switch]$Report, [switch]$Off)
$ErrorActionPreference = 'Stop'
$direct = 'D:\PhantomDust-Direct'
$log = "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"

function Set-IniValue([string]$text, [string]$key, [string]$value, [string]$comment) {
    if ($text -match "(?m)^$key=") { return $text -replace "(?m)^$key=.*$", "$key=$value" }
    return $text.TrimEnd() + "`r`n`r`n; $comment`r`n$key=$value`r`n"
}

if ($Report) {
    if (-not (Test-Path $log)) { Write-Host 'No pdnet.log yet.'; exit }
    $lines = Get-Content $log
    $start = ($lines | Select-String 'pdnet loaded from' | Select-Object -Last 1).LineNumber
    $run = $lines[($start - 1)..($lines.Count - 1)]
    Write-Host '--- pdnet answered (xsapi-pdnet)'
    $run | Where-Object { $_ -match 'xsapi-pdnet' } | ForEach-Object { $_ }
    Write-Host "`n--- stand-ins the game hit that aren't implemented yet (xsapi-stub)"
    $run | Where-Object { $_ -match 'xsapi-stub' } | ForEach-Object { $_ }
    Write-Host "`n--- EOS"
    $run | Where-Object { $_ -match ' eos: ' } | ForEach-Object { $_ }
    $p = Get-Process PDUWP -ErrorAction SilentlyContinue
    Write-Host "`nGame running now: $([bool]$p)"
    $crash = Get-WinEvent -FilterHashtable @{ LogName = 'Application'; Id = 1000; StartTime = (Get-Date).AddHours(-3) } -ErrorAction SilentlyContinue |
        Where-Object { $_.Message -match 'PDUWP' } | Select-Object -First 1
    if ($crash) { Write-Host "Last crash: $($crash.TimeCreated)"; ($crash.Message -split "`n" | Select-Object -First 8) | ForEach-Object { "  $_" } }
    exit
}

if (Get-Process PDUWP -ErrorAction SilentlyContinue) { throw 'Close Phantom Dust first.' }
$ini = Get-Content "$direct\pdnet.ini" -Raw

if ($Off) {
    Set-Content "$direct\pdnet.ini" (Set-IniValue $ini 'xbox' 'on' 'xbox=off: pdnet replaces Xbox Live entirely.') -Encoding UTF8 -NoNewline
    Write-Host 'Direct build back to Xbox Live mode (xbox=on).'
    exit
}

$creds = Get-Content "$PSScriptRoot\eos.local.ini" | Where-Object { $_ -match '^(ProductId|SandboxId|DeploymentId|ClientId|ClientSecret)=(.*)$' -and $Matches[2].Trim() }
if (@($creds).Count -lt 5) { throw 'Fill in all five values in _netmod\eos.local.ini first.' }
$sdkDll = Get-ChildItem 'C:\Users\ryhol\tools\downloads\eos-sdk' -Recurse -Filter 'EOSSDK-Win64-Shipping.dll' | Select-Object -First 1

Copy-Item "$PSScriptRoot\pdnet\build-xboxoff\Microsoft.Xbox.Services.dll" "$direct\Microsoft.Xbox.Services.dll" -Force
Copy-Item $sdkDll.FullName "$direct\EOSSDK-Win64-Shipping.dll" -Force
Copy-Item "$PSScriptRoot\eos.local.ini" "$direct\eos.ini" -Force

$ini = Set-IniValue $ini 'xbox' 'off' 'xbox=off: pdnet replaces Xbox Live entirely (no Xbox sign-in).'
$ini = Set-IniValue $ini 'eos' 'on' 'Stay signed in to Epic Online Services with this PC''s device ID (no Epic account).'
$ini = Set-IniValue $ini 'eos_test' 'off' 'EOS feasibility check (superseded by eos=on).'
if ($Name) { $ini = Set-IniValue $ini 'display_name' $Name 'Name shown to other players (max 32 characters).' }
Set-Content "$direct\pdnet.ini" $ini -Encoding UTF8 -NoNewline

& "$PSScriptRoot\builds.ps1" -Use Direct
& "$PSScriptRoot\nosuspend.ps1"
Start-Process "shell:AppsFolder\Microsoft.MSEsper_8wekyb3d8bbwe!App"
Write-Host "`nGame launched in Xbox-free mode. Try: get to the main menu, open your profile, start single player,"
Write-Host 'open Multiplayer > Xbox Live. Note where it crashes or hangs, then run:'
Write-Host '  powershell -ExecutionPolicy Bypass -File D:\PhantomDust-Dumped-NoMovie\_netmod\xboxfree.ps1 -Report'
