# Registers the Phantom Dust build in this folder with Windows (e.g. on a second PC after copying the folder).
# Replaces any other registered Phantom Dust build; save data is kept.
#   Right-click > Run with PowerShell, or:  powershell -ExecutionPolicy Bypass -File register.ps1
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$package = 'Microsoft.MSEsper'

$dev = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock' -ErrorAction SilentlyContinue
if ($dev.AllowDevelopmentWithoutDevLicense -ne 1) {
    throw 'Developer Mode is off. Turn it on in Settings > Update & Security > For developers, then run this again.'
}
if (-not (Get-AppxPackage Microsoft.VCLibs.140.00 | Where-Object Architecture -eq 'X64')) {
    throw 'Missing Microsoft.VCLibs.140.00 (x64). Install "C++ Runtime framework packages for Desktop Bridge" from Microsoft, then run this again.'
}
if (Get-Process PDUWP -ErrorAction SilentlyContinue) { throw 'Close Phantom Dust first.' }

$existing = Get-AppxPackage $package
if ($existing -and $existing.InstallLocation -ne $here) {
    Write-Host "Unregistering build at $($existing.InstallLocation) (saves are kept)"
    Remove-AppxPackage $existing.PackageFullName -PreserveApplicationData
}
Add-AppxPackage -Register "$here\AppxManifest.xml"
$p = Get-AppxPackage $package
Write-Host "Registered: $(([xml](Get-Content "$here\AppxManifest.xml")).Package.Properties.DisplayName) from $($p.InstallLocation)"
Write-Host "Launch:     explorer.exe shell:AppsFolder\$($p.PackageFamilyName)!App"
