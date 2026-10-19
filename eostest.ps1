# Runs the EOS feasibility test in the Direct build (D:\PhantomDust-Direct). The Teredo build is not touched.
#   .\eostest.ps1            prepare the Direct build, register it, launch the game
#   .\eostest.ps1 -Report    show the eos-test results from pdnet.log
# Afterwards switch back with:  .\builds.ps1 -Use Teredo
param([switch]$Report)
$ErrorActionPreference = 'Stop'
$direct = 'D:\PhantomDust-Direct'
$log = "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"

if ($Report) {
    if (-not (Test-Path $log)) { Write-Host 'No pdnet.log yet.'; exit }
    $lines = Get-Content $log
    $start = ($lines | Select-String 'eos-test: starting' | Select-Object -Last 1).LineNumber
    if (-not $start) { Write-Host 'No eos-test run found in pdnet.log.'; exit }
    $lines[($start - 1)..($lines.Count - 1)] | Where-Object { $_ -match 'eos-test|eos-sdk' } | ForEach-Object { $_ }
    exit
}

if (Get-Process PDUWP -ErrorAction SilentlyContinue) { throw 'Close Phantom Dust first.' }

$creds = Get-Content "$PSScriptRoot\eos.local.ini" | Where-Object { $_ -match '^(ProductId|SandboxId|DeploymentId|ClientId|ClientSecret)=(.*)$' -and $Matches[2].Trim() }
if (@($creds).Count -lt 5) { throw 'Fill in all five values in _netmod\eos.local.ini first.' }

$sdkDll = Get-ChildItem 'C:\Users\ryhol\tools\downloads\eos-sdk' -Recurse -Filter 'EOSSDK-Win64-Shipping.dll' | Select-Object -First 1
if (-not $sdkDll) { throw 'EOSSDK-Win64-Shipping.dll not found under C:\Users\ryhol\tools\downloads\eos-sdk.' }

Copy-Item "$PSScriptRoot\pdnet\build-eostest\Microsoft.Xbox.Services.dll" "$direct\Microsoft.Xbox.Services.dll" -Force
Copy-Item $sdkDll.FullName "$direct\EOSSDK-Win64-Shipping.dll" -Force
Copy-Item "$PSScriptRoot\eos.local.ini" "$direct\eos.ini" -Force

$ini = Get-Content "$direct\pdnet.ini" -Raw
if ($ini -match '(?m)^eos_test=') { $ini = $ini -replace '(?m)^eos_test=.*$', 'eos_test=on' }
else { $ini = $ini.TrimEnd() + "`r`n`r`n; EOS feasibility check (results in pdnet.log as eos-test: lines).`r`neos_test=on`r`n" }
Set-Content "$direct\pdnet.ini" $ini -Encoding ASCII -NoNewline

& "$PSScriptRoot\builds.ps1" -Use Direct
Start-Process "shell:AppsFolder\Microsoft.MSEsper_8wekyb3d8bbwe!App"
Write-Host "`nGame launched. Wait at the title/main menu for about 30 seconds, then run:"
Write-Host '  powershell -ExecutionPolicy Bypass -File D:\PhantomDust-Dumped-NoMovie\_netmod\eostest.ps1 -Report'
Write-Host 'Afterwards, close the game and switch back:'
Write-Host '  powershell -ExecutionPolicy Bypass -File D:\PhantomDust-Dumped-NoMovie\_netmod\builds.ps1 -Use Teredo'
