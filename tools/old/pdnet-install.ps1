# Installs the pdnet shim into the game folder. Run uninstall.ps1 to undo.
$ErrorActionPreference = 'Stop'
$game = Resolve-Path "$PSScriptRoot\..\.."
$backup = "$PSScriptRoot\..\original"
$built = "$PSScriptRoot\build\Microsoft.Xbox.Services.dll"

if (Get-Process PDUWP -ErrorAction SilentlyContinue) { throw 'Close Phantom Dust first.' }
if (-not (Test-Path $built)) { throw "Build first: $built not found." }

# One-time backup of the untouched files.
New-Item -ItemType Directory -Force $backup | Out-Null
foreach ($f in 'Microsoft.Xbox.Services.dll', 'AppxManifest.xml') {
    if (-not (Test-Path "$backup\$f") -and -not ($f -eq 'Microsoft.Xbox.Services.dll' -and (Test-Path "$game\Microsoft.Xbox.Services.orig.dll"))) {
        Copy-Item "$game\$f" "$backup\$f"
    }
}

if (-not (Test-Path "$game\Microsoft.Xbox.Services.orig.dll")) {
    Rename-Item "$game\Microsoft.Xbox.Services.dll" 'Microsoft.Xbox.Services.orig.dll'
}
Copy-Item $built "$game\Microsoft.Xbox.Services.dll" -Force
Write-Host "pdnet installed. Log: $env:LOCALAPPDATA\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log"
