# Builds the stage B tester kit: D:\PhantomDust-pdnet-StageB-TestKit-<date>.zip (mod only, no game files).
#   .\make-testkit-stageb.ps1                          kit WITHOUT eos.ini (send the tester an eos.ini separately)
#   .\make-testkit-stageb.ps1 -EosIni D:\tester-eos.ini   kit with that eos.ini inside
# eos.ini holds an EOS client id and secret. Use a client made for testers in the Epic portal, not the
# development client in _netmod\eos.local.ini (see HANDOFF §9).
param([string]$EosIni, [string]$Build = 'build-stageb')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$stamp = Get-Date -Format 'yyyy-MM-dd'
$name = "PhantomDust-pdnet-StageB-TestKit-$stamp"
$out = "D:\$name"
$zip = "D:\$name.zip"

$dll = "$root\pdnet\$Build\Microsoft.Xbox.Services.dll"
if (-not (Test-Path $dll)) { throw "No DLL at $dll. Build it first: pdnet\build.cmd $Build" }
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
New-Item -ItemType Directory "$out\pdnet" | Out-Null
Copy-Item "$root\testkit-stageb\*" $out
Copy-Item $dll "$out\pdnet\"
Copy-Item $sdkDll.FullName $out
if ($EosIni) { Copy-Item $EosIni "$out\eos.ini" }

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$out\*" -DestinationPath $zip
$md5 = (Get-FileHash $dll -Algorithm MD5).Hash
Write-Host "Kit: $zip  (pdnet DLL MD5 $md5)"
if ($EosIni) { Write-Host 'eos.ini is INSIDE the zip: share it only with your testers.' -ForegroundColor Yellow }
else { Write-Host 'eos.ini is NOT in the zip: send each tester an eos.ini separately (they put it next to install.ps1).' -ForegroundColor Yellow }
