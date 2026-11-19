# Copies the pdnet log (and pdlaunch.log from this folder) to your Desktop so you can send them after a test,
# and prints a short summary of the lobby steps.
$log = "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"
$launch = Join-Path $PSScriptRoot 'pdlaunch.log'
$desktop = [Environment]::GetFolderPath('Desktop')
# Named after the in-game name (display_name: pdnet.ini, then the game's settings.ini), never the PC's own names, and
# the copies are cleaned of the Windows user name and the computer name too (the user, 2026-11-22).
$player = 'player'
foreach ($ini in @((Join-Path $PSScriptRoot 'pdnet.ini'), "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet\settings.ini")) {
    if (Test-Path $ini) {
        $line = Select-String -Path $ini -Pattern '^display_name=(.+)$' -Encoding UTF8 | Select-Object -Last 1
        if ($line) { $player = $line.Matches[0].Groups[1].Value.Trim() }
    }
}
if ($player.Length -gt 16) { $player = $player.Substring(0, 16) }
$safe = ($player -replace '[^A-Za-z0-9_-]', '_'); if (-not $safe) { $safe = 'player' }
$stamp = '{0}-{1:yyyyMMdd-HHmm}' -f $safe, (Get-Date)
function Copy-Clean($from, $to) {
    $in = [IO.File]::Open($from, 'Open', 'Read', 'ReadWrite') # the game keeps its log open while it runs
    try { $text = (New-Object IO.StreamReader($in, [Text.Encoding]::UTF8)).ReadToEnd() } finally { $in.Dispose() }
    $text = $text -replace ('(?i)(\\Users\\)' + [regex]::Escape($env:USERNAME) + '(?=\\)'), ('$1' + $player)
    $text = $text -replace ('(?i)\b' + [regex]::Escape($env:COMPUTERNAME) + '\b'), $player
    [IO.File]::WriteAllText($to, $text, (New-Object Text.UTF8Encoding $false))
}
if (Test-Path $launch) { Copy-Clean $launch (Join-Path $desktop "pdlaunch-log-$stamp.txt") }
if (-not (Test-Path $log)) {
    Write-Host 'No pdnet.log yet. Start Phantom Dust once after Setup.bat.'
    if (Test-Path $launch) { Write-Host "`npdlaunch.log (copied to your Desktop):"; Get-Content $launch -Tail 10 | ForEach-Object { "  $_" } }
    Read-Host 'Press Enter to close'
    exit
}
$out = Join-Path $desktop "pdnet-log-$stamp.txt"
Copy-Clean $log $out

$lines = Get-Content $out
$start = ($lines | Select-String 'pdnet loaded from' | Select-Object -Last 1).LineNumber
if ($start) {
    Write-Host "`nLast game run, lobby and connection steps:"
    $lines[($start - 1)..($lines.Count - 1)] |
        Where-Object { $_ -match 'pdnet loaded|gamepatch|eos: signed in|eos: .*fail|lobby .* created|joined lobby|could not|left lobby|changed remotely|search .*session|sda: (connect|inbound|.*TIMED|EOS)|lobby: |p2p: |p2p-debug: ' } |
        Select-Object -Last 40 | ForEach-Object { "  $_" }
}
Write-Host "`nSaved $out$(if (Test-Path $launch) { ' and the pdlaunch log' }) on your Desktop - send them back."
Read-Host 'Press Enter to close'
