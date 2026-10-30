# Copies the pdnet log to your Desktop so you can send it after a test, and prints a short summary of the lobby steps.
$log = "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"
if (-not (Test-Path $log)) { Write-Host 'No pdnet.log yet - run the game with pdnet installed first.'; Read-Host 'Press Enter to close'; exit }
$out = Join-Path ([Environment]::GetFolderPath('Desktop')) ("pdnet-log-{0}-{1:yyyyMMdd-HHmm}.txt" -f $env:COMPUTERNAME, (Get-Date))
# The game keeps the log open while it runs, so read it with sharing allowed.
$in = [IO.File]::Open($log, 'Open', 'Read', 'ReadWrite')
try { $copy = [IO.File]::Create($out); try { $in.CopyTo($copy) } finally { $copy.Dispose() } } finally { $in.Dispose() }

$lines = Get-Content $out
$start = ($lines | Select-String 'pdnet loaded from' | Select-Object -Last 1).LineNumber
if ($start) {
    Write-Host "`nLast game run, lobby and connection steps:"
    $lines[($start - 1)..($lines.Count - 1)] |
        Where-Object { $_ -match 'eos: signed in|eos: .*fail|lobby .* created|joined lobby|could not|left lobby|changed remotely|search .*session|sda: (connect|inbound|.*TIMED|EOS)|lobby: |p2p: ' } |
        Select-Object -Last 40 | ForEach-Object { "  $_" }
}
Write-Host "`nSaved $out - send this file back."
Read-Host 'Press Enter to close'
