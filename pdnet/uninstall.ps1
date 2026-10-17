# Restores the original Microsoft.Xbox.Services.dll. Local saves under LocalState\pdnet are kept.
$ErrorActionPreference = 'Stop'
$game = Resolve-Path "$PSScriptRoot\..\.."

if (Get-Process PDUWP -ErrorAction SilentlyContinue) { throw 'Close Phantom Dust first.' }
if (-not (Test-Path "$game\Microsoft.Xbox.Services.orig.dll")) { Write-Host 'pdnet is not installed.'; return }

Remove-Item "$game\Microsoft.Xbox.Services.dll" -Force
Rename-Item "$game\Microsoft.Xbox.Services.orig.dll" 'Microsoft.Xbox.Services.dll'
Write-Host 'Original Xbox Live DLL restored.'
