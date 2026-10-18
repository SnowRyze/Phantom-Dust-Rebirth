# Records Teredo state and connection quality while you play, to help explain lag in online matches.
# Read-only: it only runs netsh "show" and pings. Start it before a match, press Ctrl+C when done.
#   powershell -ExecutionPolicy Bypass -File D:\PhantomDust-Dumped-NoMovie\_netmod\lagmonitor.ps1
# Writes a CSV to your Desktop. Note the clock time whenever a lag spike happens and send both.
param([int]$IntervalSeconds = 3)

$out = Join-Path ([Environment]::GetFolderPath('Desktop')) ("pd-lagmonitor-{0:yyyyMMdd-HHmm}.csv" -f (Get-Date))
'time,teredo_state,teredo_nat,teredo_external,ping_ms,lost_of_4,game_running' | Set-Content $out -Encoding ASCII
Write-Host "Recording to $out every $IntervalSeconds s. Press Ctrl+C to stop."

$lastState = ''
while ($true) {
    $t = netsh interface teredo show state
    $state = (($t | Select-String '^State\s*:') -replace '^State\s*:\s*', '').Trim()
    $nat = (($t | Select-String '^NAT\s*:') -replace '^NAT\s*:\s*', '').Trim()
    $ext = (($t | Select-String '^External NAT Mapping\s*:') -replace '^External NAT Mapping\s*:\s*', '').Trim()

    $replies = @(Test-Connection win1910.ipv6.microsoft.com -Count 4 -ErrorAction SilentlyContinue)
    $lost = 4 - $replies.Count
    $ping = if ($replies.Count) { [math]::Round(($replies | Measure-Object ResponseTime -Average).Average) } else { '' }
    $game = [bool](Get-Process PDUWP -ErrorAction SilentlyContinue)

    $now = Get-Date -Format 'HH:mm:ss'
    "$now,$state,""$nat"",$ext,$ping,$lost,$game" | Add-Content $out -Encoding ASCII

    $flag = if ($state -ne 'qualified' -or $lost -gt 0 -or ($ping -and $ping -gt 80)) { '  <-- check' } else { '' }
    if ($state -ne $lastState) { Write-Host "$now Teredo state: $state ($nat)" -ForegroundColor Yellow; $lastState = $state }
    Write-Host ("{0}  ping {1,4} ms  lost {2}/4  game {3}{4}" -f $now, $ping, $lost, $(if ($game) { 'on' } else { 'off' }), $flag)
    Start-Sleep -Seconds $IntervalSeconds
}
