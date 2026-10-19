# Runs the Direct build with the long-lived EOS sign-in (pdnet.ini: eos=on). The Teredo build is not touched.
#   .\eosrun.ps1 -Name "MyName"   prepare D:\PhantomDust-Direct, register it, keep it running when minimized, launch
#   .\eosrun.ps1 -Report          show the eos: lines from the latest run
# Afterwards switch back with:  .\builds.ps1 -Use Teredo
param([string]$Name, [switch]$Report)
$ErrorActionPreference = 'Stop'
$direct = 'D:\PhantomDust-Direct'
$log = "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"

if ($Report) {
    if (-not (Test-Path $log)) { Write-Host 'No pdnet.log yet.'; exit }
    $lines = Get-Content $log
    $start = ($lines | Select-String 'eos: starting' | Select-Object -Last 1).LineNumber
    if (-not $start) { Write-Host 'No eos run found in pdnet.log.'; exit }
    $lines[($start - 1)..($lines.Count - 1)] | Where-Object { $_ -match ' eos: |eos-sdk|credstore' } | ForEach-Object { $_ }
    exit
}

if (Get-Process PDUWP -ErrorAction SilentlyContinue) { throw 'Close Phantom Dust first.' }
$creds = Get-Content "$PSScriptRoot\eos.local.ini" | Where-Object { $_ -match '^(ProductId|SandboxId|DeploymentId|ClientId|ClientSecret)=(.*)$' -and $Matches[2].Trim() }
if (@($creds).Count -lt 5) { throw 'Fill in all five values in _netmod\eos.local.ini first.' }
$sdkDll = Get-ChildItem 'C:\Users\ryhol\tools\downloads\eos-sdk' -Recurse -Filter 'EOSSDK-Win64-Shipping.dll' | Select-Object -First 1
if (-not $sdkDll) { throw 'EOSSDK-Win64-Shipping.dll not found under C:\Users\ryhol\tools\downloads\eos-sdk.' }

Copy-Item "$PSScriptRoot\pdnet\build-eos\Microsoft.Xbox.Services.dll" "$direct\Microsoft.Xbox.Services.dll" -Force
Copy-Item $sdkDll.FullName "$direct\EOSSDK-Win64-Shipping.dll" -Force
Copy-Item "$PSScriptRoot\eos.local.ini" "$direct\eos.ini" -Force

function Set-IniValue([string]$text, [string]$key, [string]$value, [string]$comment) {
    if ($text -match "(?m)^$key=") { return $text -replace "(?m)^$key=.*$", "$key=$value" }
    return $text.TrimEnd() + "`r`n`r`n; $comment`r`n$key=$value`r`n"
}
$ini = Get-Content "$direct\pdnet.ini" -Raw
$ini = Set-IniValue $ini 'eos' 'on' 'Stay signed in to Epic Online Services with this PC''s device ID (no Epic account).'
$ini = Set-IniValue $ini 'eos_test' 'off' 'EOS feasibility check (superseded by eos=on).'
if ($Name) { $ini = Set-IniValue $ini 'display_name' $Name 'Name shown to other players (max 32 characters).' }
Set-Content "$direct\pdnet.ini" $ini -Encoding UTF8 -NoNewline

& "$PSScriptRoot\builds.ps1" -Use Direct
& "$PSScriptRoot\nosuspend.ps1"
Start-Process "shell:AppsFolder\Microsoft.MSEsper_8wekyb3d8bbwe!App"
Write-Host "`nGame launched. Play or idle as usual (minimizing is fine). Later, run:"
Write-Host '  powershell -ExecutionPolicy Bypass -File D:\PhantomDust-Dumped-NoMovie\_netmod\eosrun.ps1 -Report'
