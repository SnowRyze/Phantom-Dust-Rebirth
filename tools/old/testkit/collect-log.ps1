# Copies the pdnet log to your Desktop so you can send it after a test.
$log = "$env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"
if (-not (Test-Path $log)) { Write-Host 'No pdnet.log yet - run the game with pdnet installed first.'; Read-Host 'Press Enter to close'; exit }
$out = Join-Path ([Environment]::GetFolderPath('Desktop')) ("pdnet-log-{0}-{1:yyyyMMdd-HHmm}.txt" -f $env:COMPUTERNAME, (Get-Date))
[IO.File]::WriteAllBytes($out, [IO.File]::ReadAllBytes($log))
Write-Host "Saved $out - send this file back."
Read-Host 'Press Enter to close'
