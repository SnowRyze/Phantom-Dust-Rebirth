# Copies the pdnet log (and pdlaunch.log from this folder) to your Desktop so you can send them after a test,
# and prints a short summary of the lobby steps.
$log = "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"
$launch = Join-Path $PSScriptRoot 'pdlaunch.log'
$desktop = [Environment]::GetFolderPath('Desktop')
$stamp = '{0}-{1:yyyyMMdd-HHmm}' -f $env:COMPUTERNAME, (Get-Date)
if (Test-Path $launch) { Copy-Item $launch (Join-Path $desktop "pdlaunch-log-$stamp.txt") }
if (-not (Test-Path $log)) {
    Write-Host 'No pdnet.log yet. Start Phantom Dust once after Setup.bat.'
    if (Test-Path $launch) { Write-Host "`npdlaunch.log (copied to your Desktop):"; Get-Content $launch -Tail 10 | ForEach-Object { "  $_" } }
    Read-Host 'Press Enter to close'
    exit
}
$out = Join-Path $desktop "pdnet-log-$stamp.txt"
[IO.File]::WriteAllBytes($out, [IO.File]::ReadAllBytes($log))

$lines = Get-Content $out
$start = ($lines | Select-String 'pdnet loaded from' | Select-Object -Last 1).LineNumber
if ($start) {
    Write-Host "`nLast game run, lobby and connection steps:"
    $lines[($start - 1)..($lines.Count - 1)] |
        Where-Object { $_ -match 'pdnet loaded|gamepatch|eos: signed in|eos: .*fail|lobby .* created|joined lobby|could not|left lobby|changed remotely|search .*session|sda: (connect|inbound|.*TIMED|EOS)|lobby: |p2p: ' } |
        Select-Object -Last 40 | ForEach-Object { "  $_" }
}
Write-Host "`nSaved $out$(if (Test-Path $launch) { ' and the pdlaunch log' }) on your Desktop - send them back."
Read-Host 'Press Enter to close'
