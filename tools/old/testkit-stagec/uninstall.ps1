# Removes the pdnet stage C test build from your Phantom Dust folder and restores the original files.
#   powershell -ExecutionPolicy Bypass -File uninstall.ps1 -GameFolder "D:\Games\PhantomDust"
# Local save data (LocalState\pdnet\saves) is kept.
param([string]$GameFolder)
$ErrorActionPreference = 'Stop'
$package = 'Microsoft.MSEsper'

if (-not $GameFolder) { $GameFolder = Read-Host 'Path to your Phantom Dust folder' }
$GameFolder = $GameFolder.Trim('"').TrimEnd('\')
$backup = "$GameFolder\_pdnet_backup"
if (-not (Test-Path $backup)) { Write-Host 'No pdnet backup found here; nothing to undo.'; Read-Host 'Press Enter to close'; exit }
if (Get-Process PDUWP -ErrorAction SilentlyContinue) { Write-Host 'Close Phantom Dust first.' -ForegroundColor Red; Read-Host 'Press Enter to close'; exit 1 }

Copy-Item "$backup\Microsoft.Xbox.Services.dll" "$GameFolder\Microsoft.Xbox.Services.dll" -Force
Remove-Item "$GameFolder\Microsoft.Xbox.Services.orig.dll" -ErrorAction SilentlyContinue
Copy-Item "$backup\AppxManifest.xml" "$GameFolder\AppxManifest.xml" -Force
Remove-Item "$GameFolder\EOSSDK-Win64-Shipping.dll", "$GameFolder\eos.ini" -ErrorAction SilentlyContinue
if (Test-Path "$backup\pdnet.ini") { Copy-Item "$backup\pdnet.ini" "$GameFolder\pdnet.ini" -Force }
else { Remove-Item "$GameFolder\pdnet.ini" -ErrorAction SilentlyContinue }

$existing = Get-AppxPackage $package
if ($existing -and $existing.InstallLocation -eq $GameFolder) {
    try { & (Join-Path $PSScriptRoot 'nosuspend.ps1') -Off | Out-Null } catch {}
    Remove-AppxPackage $existing.PackageFullName -PreserveApplicationData
    Add-AppxPackage -Register "$GameFolder\AppxManifest.xml"
}
Remove-Item $backup -Recurse -Force
Write-Host 'Original files restored. Note: without pdnet, this copy may hang on "retrieving profile" (Xbox cloud saves).'
Read-Host 'Press Enter to close'
