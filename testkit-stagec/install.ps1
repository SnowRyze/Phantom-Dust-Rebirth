# Installs the pdnet stage C test build into YOUR OWN unpacked copy of Phantom Dust (2017, PC).
# Usage: right-click > Run with PowerShell, or
#   powershell -ExecutionPolicy Bypass -File install.ps1 -GameFolder "D:\Games\PhantomDust" -Name "MyName"
# Undo with uninstall.ps1. Nothing outside the game folder is changed except the app registration
# and Windows' "don't suspend this app" setting for Phantom Dust (so it keeps running when minimized).
param([string]$GameFolder, [string]$Name)
$ErrorActionPreference = 'Stop'
$kit = $PSScriptRoot
$package = 'Microsoft.MSEsper'
$testedVersion = '1.3.25854.2'

function Fail($msg) { Write-Host "`nERROR: $msg" -ForegroundColor Red; Read-Host 'Press Enter to close'; exit 1 }

foreach ($f in 'pdnet\Microsoft.Xbox.Services.dll', 'EOSSDK-Win64-Shipping.dll', 'pdnet.ini') {
    if (-not (Test-Path "$kit\$f")) { Fail "The kit is incomplete: $f is missing. Unzip the whole kit and run install.ps1 from that folder." }
}
if (-not (Test-Path "$kit\eos.ini")) { Fail 'eos.ini is missing from the kit folder. The person who sent you the kit sends it separately; put it next to install.ps1.' }
$creds = Get-Content "$kit\eos.ini" | Where-Object { $_ -match '^(ProductId|SandboxId|DeploymentId|ClientId|ClientSecret)=(.+)$' }
if (@($creds).Count -lt 5) { Fail 'eos.ini is incomplete (it needs ProductId, SandboxId, DeploymentId, ClientId and ClientSecret).' }

if (-not $GameFolder) { $GameFolder = Read-Host 'Path to your Phantom Dust folder (the one containing PDUWP.exe and AppxManifest.xml)' }
$GameFolder = $GameFolder.Trim('"').TrimEnd('\')
if (-not (Test-Path "$GameFolder\AppxManifest.xml") -or -not (Test-Path "$GameFolder\PDUWP.exe")) { Fail "No PDUWP.exe / AppxManifest.xml in '$GameFolder'." }
if ($GameFolder -like '*\WindowsApps\*') { Fail 'This is the Microsoft Store install, which is encrypted and read-only. pdnet needs an unpacked (dumped) copy of the game.' }

# --- checks ---------------------------------------------------------------------------------------
$manifest = [xml](Get-Content "$GameFolder\AppxManifest.xml")
$id = $manifest.Package.Identity
if ($id.Name -ne $package) { Fail "This doesn't look like Phantom Dust (package name '$($id.Name)')." }
$head = [IO.File]::ReadAllBytes("$GameFolder\PDUWP.exe")[0..1]
if ($head[0] -ne 0x4D -or $head[1] -ne 0x5A) { Fail 'PDUWP.exe is not readable as a normal program (still encrypted?). pdnet needs an unpacked copy.' }
if ($id.Version -ne $testedVersion) {
    Write-Host "Warning: your game is version $($id.Version); pdnet was built against $testedVersion." -ForegroundColor Yellow
    if ((Read-Host 'Continue anyway? (y/n)') -ne 'y') { exit 1 }
}
$dev = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock' -ErrorAction SilentlyContinue
if ($dev.AllowDevelopmentWithoutDevLicense -ne 1) { Fail 'Developer Mode is off. Turn it on in Settings > (Update & Security >) For developers, then run this again.' }
if (-not (Get-AppxPackage Microsoft.VCLibs.140.00 | Where-Object Architecture -eq 'X64')) {
    Fail 'Missing Microsoft.VCLibs.140.00 (x64). Install Microsoft''s "C++ Runtime framework packages for Desktop Bridge", then run this again.'
}
if (Get-Process PDUWP -ErrorAction SilentlyContinue) { Fail 'Close Phantom Dust first.' }

if (-not $Name) { $Name = Read-Host 'Your player name (what the other tester will see, max 32 characters)' }
$Name = ($Name -replace '[\r\n=$]', '').Trim()
if (-not $Name) { $Name = $env:COMPUTERNAME }
if ($Name.Length -gt 32) { $Name = $Name.Substring(0, 32) }

# --- one-time backup of the files we change -------------------------------------------------------
$backup = "$GameFolder\_pdnet_backup"
New-Item -ItemType Directory -Force $backup | Out-Null
$xsapi = "$GameFolder\Microsoft.Xbox.Services.dll"
$orig = "$GameFolder\Microsoft.Xbox.Services.orig.dll"
if (-not (Test-Path $orig)) {
    if ((Get-Item $xsapi).Length -lt 5MB) { Fail 'Microsoft.Xbox.Services.dll is not the original Xbox Live library and no .orig.dll exists. Restore the original file first.' }
    Copy-Item $xsapi "$backup\Microsoft.Xbox.Services.dll"
    Copy-Item $xsapi $orig
}
if (-not (Test-Path "$backup\AppxManifest.xml")) { Copy-Item "$GameFolder\AppxManifest.xml" "$backup\AppxManifest.xml" }
if ((Test-Path "$GameFolder\pdnet.ini") -and -not (Test-Path "$backup\pdnet.ini")) { Copy-Item "$GameFolder\pdnet.ini" "$backup\pdnet.ini" }

# --- install --------------------------------------------------------------------------------------
Copy-Item "$kit\pdnet\Microsoft.Xbox.Services.dll" $xsapi -Force
Copy-Item "$kit\EOSSDK-Win64-Shipping.dll" "$GameFolder\EOSSDK-Win64-Shipping.dll" -Force
Copy-Item "$kit\eos.ini" "$GameFolder\eos.ini" -Force
$ini = Get-Content "$kit\pdnet.ini" -Raw
$ini = $ini -replace '(?m)^display_name=.*$', "display_name=$Name"
Set-Content "$GameFolder\pdnet.ini" $ini -Encoding UTF8 -NoNewline

# Manifest (from the backup, so re-running is safe): label it, and let Windows deliver incoming UDP to the game.
$xml = Get-Content "$backup\AppxManifest.xml" -Raw
$xml = $xml.Replace('<DisplayName>Phantom Dust</DisplayName>', '<DisplayName>Phantom Dust (pdnet test)</DisplayName>')
$xml = $xml.Replace('DisplayName="Phantom Dust"', 'DisplayName="Phantom Dust (pdnet test)"')
if ($xml -notmatch 'internetClientServer') {
    $xml = $xml.Replace('<Capability Name="internetClient" />',
        '<Capability Name="internetClient" /><Capability Name="internetClientServer" /><Capability Name="privateNetworkClientServer" />')
}
Set-Content "$GameFolder\AppxManifest.xml" $xml -Encoding UTF8 -NoNewline

# --- register with Windows ------------------------------------------------------------------------
# Windows only reads AppxManifest.xml when the game is registered, and it refuses to register a changed manifest over an
# existing registration (0x80073CFB), even from the same folder. So any earlier registration is removed first.
$existing = Get-AppxPackage $package
if ($existing) {
    if ($existing.SignatureKind -eq 'Store') {
        Write-Host "The Microsoft Store version of Phantom Dust is installed. It has to be uninstalled to install this copy." -ForegroundColor Yellow
        if ((Read-Host 'Uninstall the Store version now? (y/n)') -ne 'y') { exit 1 }
        Remove-AppxPackage $existing.PackageFullName
    } else {
        Write-Host "Replacing the existing Phantom Dust install from $($existing.InstallLocation) (save data is kept)."
        Remove-AppxPackage $existing.PackageFullName -PreserveApplicationData
    }
}
try { Add-AppxPackage -Register "$GameFolder\AppxManifest.xml" }
catch { Fail "Windows refused to install the game: $($_.Exception.Message)" }
$p = Get-AppxPackage $package

# Windows pauses Store apps when they're minimized, which drops you from the lobby. Always switch that off.
try { & "$kit\nosuspend.ps1" | Out-Null; Write-Host 'Phantom Dust will keep running when minimized or in the background.' }
catch { Write-Host "Warning: could not stop Windows from pausing the game when minimized ($_). Alt-tabbing may drop you from lobbies; run nosuspend.ps1 later to retry." -ForegroundColor Yellow }

Write-Host "`nInstalled the pdnet stage C test build in $GameFolder as '$Name'" -ForegroundColor Green
Write-Host "Start menu: 'Phantom Dust (pdnet test)'   or run:  explorer.exe shell:AppsFolder\$($p.PackageFamilyName)!App"
Write-Host "Settings:   $GameFolder\pdnet.ini"
Write-Host "Log:        $env:LOCALAPPDATA\Packages\$($p.PackageFamilyName)\LocalState\pdnet.log  (collect-log.ps1 copies it to your Desktop)"
Read-Host 'Press Enter to close'
