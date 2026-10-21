# Builds a tester kit (mod only, no game files) from testkit-stage<b|c>\ plus a pdnet build:
#   D:\PhantomDust-pdnet-Stage<B|C>-TestKit-<DLL MD5 prefix>[-with-eos].zip
# The name carries the DLL's fingerprint (not the date: this PC's clock jumps) and says whether eos.ini is inside.
#   .\make-testkit.ps1 -Stage C -EosIni .\eos.tester.ini     stage C kit (EOS P2P), credentials inside
#   .\make-testkit.ps1 -Stage B                              stage B kit (direct UDP), no eos.ini (send it separately)
#   -Build <folder under pdnet>   default build-stageb-tested (B) / build-stagec (C)
# eos.ini holds an EOS client id and secret. Use the tester client (eos.tester.ini), not the development client
# in _netmod\eos.local.ini (see HANDOFF §9).
#   -Webhook <Discord webhook URL>   stage C: players' games announce the lobbies they host there
#            (default: webhook_url from pdnet\tools\out\lobbywatch.ini; -Webhook none = no Discord messages)
param([ValidateSet('B', 'C')][string]$Stage = 'C', [string]$EosIni, [string]$Build, [string]$Webhook)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $Build) { $Build = if ($Stage -eq 'B') { 'build-stageb-tested' } else { 'build-stagec' } }
$kitSource = "$root\testkit-stage$($Stage.ToLower())"

$dll = "$root\pdnet\$Build\Microsoft.Xbox.Services.dll"
if (-not (Test-Path $dll)) { throw "No DLL at $dll. Build it first: pdnet\build.cmd $Build" }
$md5 = (Get-FileHash $dll -Algorithm MD5).Hash
$name = "PhantomDust-pdnet-Stage$Stage-TestKit-$($md5.Substring(0, 8))" + $(if ($EosIni) { '-with-eos' } else { '' })
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
New-Item -ItemType Directory "$out\pdnet" | Out-Null
Copy-Item "$kitSource\*" $out
Copy-Item $dll "$out\pdnet\"
Copy-Item $sdkDll.FullName $out
if ($EosIni) { Copy-Item $EosIni "$out\eos.ini" }
if ($Stage -eq 'C') {
    $watchIni = "$root\pdnet\tools\out\lobbywatch.ini"
    if (-not $Webhook -and (Test-Path $watchIni)) {
        $line = Select-String -Path $watchIni -Pattern '^webhook_url=(.+)$' | Select-Object -First 1
        if ($line) { $Webhook = $line.Matches[0].Groups[1].Value.Trim() }
    }
    if ($Webhook -eq 'none') { $Webhook = '' }
    $ini = (Get-Content "$out\pdnet.ini" -Raw) -replace '(?m)^discord_webhook=.*$', "discord_webhook=$Webhook"
    Set-Content "$out\pdnet.ini" $ini -Encoding UTF8 -NoNewline
}

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$out\*" -DestinationPath $zip
Write-Host "Kit: $zip  (stage $Stage, pdnet DLL MD5 $md5 from $Build)"
if ($Stage -eq 'C') {
    if ($Webhook) { Write-Host 'The Discord webhook URL is INSIDE the zip (pdnet.ini): anyone with the kit can post to that channel.' -ForegroundColor Yellow }
    else { Write-Host 'No Discord webhook in this kit (lobbies are not announced).' }
}
if ($EosIni) { Write-Host 'eos.ini is INSIDE the zip: share it only with your testers.' -ForegroundColor Yellow }
else { Write-Host 'eos.ini is NOT in the zip: send each tester an eos.ini separately (they put it next to install.ps1).' -ForegroundColor Yellow }
