# Creates the two playable builds from the master dump, and switches which one Windows has registered.
#
#   .\builds.ps1 -Create              copy the master into D:\PhantomDust-Direct and D:\PhantomDust-Teredo
#   .\builds.ps1 -Use Direct          register the Direct build (pdnet direct UDP, no Teredo)
#   .\builds.ps1 -Use Teredo          register the Teredo build (original Microsoft networking)
#   .\builds.ps1 -Use Master          register the untouched master dump
#   .\builds.ps1                      show which build is registered
#
# All builds keep the original package identity (Xbox Live sign-in is tied to it), so only one can be
# registered at a time. They share the same save data; switching keeps it.
param([switch]$Create, [ValidateSet('Direct', 'Teredo', 'Master')][string]$Use)
$ErrorActionPreference = 'Stop'

$master = Resolve-Path "$PSScriptRoot\.."
$orig = "$PSScriptRoot\original"
$shim = "$PSScriptRoot\pdnet\build\Microsoft.Xbox.Services.dll"
$folders = @{ Direct = 'D:\PhantomDust-Direct'; Teredo = 'D:\PhantomDust-Teredo'; Master = "$master" }
$package = 'Microsoft.MSEsper'

function New-Build([string]$name, [string]$mode, [bool]$inbound) {
    $dir = $folders[$name]
    Write-Host "== $name -> $dir"
    robocopy "$master" $dir /E /XD "$master\_netmod" /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }

    # Xbox Live DLL: pdnet in front, the original behind it.
    Copy-Item "$orig\Microsoft.Xbox.Services.dll" "$dir\Microsoft.Xbox.Services.orig.dll" -Force
    Copy-Item $shim "$dir\Microsoft.Xbox.Services.dll" -Force

    # Menus: Options = real options, Multiplayer > Split Screen = debug menu.
    Copy-Item "$dir\Assets\Data\Menu\option_enx.ssb" "$dir\Assets\Data\Menu\option_en.ssb" -Force
    Copy-Item "$orig\menu\option_en.ssb" "$dir\Assets\Data\Menu\vsmode_en.ssb" -Force

    # Manifest: label the build; let Windows deliver inbound UDP when pdnet listens for peers.
    $xml = Get-Content "$orig\AppxManifest.xml" -Raw
    $xml = $xml.Replace('<DisplayName>Phantom Dust</DisplayName>', "<DisplayName>Phantom Dust ($name)</DisplayName>")
    $xml = $xml.Replace('DisplayName="Phantom Dust"', "DisplayName=""Phantom Dust ($name)""")
    if ($inbound) {
        $xml = $xml.Replace('<Capability Name="internetClient" />',
            '<Capability Name="internetClient" /><Capability Name="internetClientServer" /><Capability Name="privateNetworkClientServer" />')
    }
    Set-Content "$dir\AppxManifest.xml" $xml -Encoding UTF8 -NoNewline

    # One-step registration for copying this build to another PC.
    Copy-Item "$PSScriptRoot\register.ps1" "$dir\register.ps1" -Force

    # Build-specific pdnet settings (takes precedence over the shared one in LocalState).
    @"
; pdnet settings for the $name build. Restart the game after editing.

; direct = pdnet direct UDP connections (no Teredo)
; teredo = original Microsoft Teredo / Xbox Secure Device Association
network_mode=$mode

; Address other players use to reach this PC (direct mode): auto, or a fixed IP (e.g. public IP when port-forwarding).
advertise_ip=auto

; UDP ports (direct mode). game_port must match networkmanifest.xml; forward both for internet play.
game_port=43210
control_port=43211

qos_probes=5
"@ | Set-Content "$dir\pdnet.ini" -Encoding ASCII
}

function Get-Registered {
    $p = Get-AppxPackage $package
    if (-not $p) { return $null }
    foreach ($k in $folders.Keys) { if ($p.InstallLocation -eq $folders[$k]) { return $k } }
    return $p.InstallLocation
}

if (Get-Process PDUWP -ErrorAction SilentlyContinue) { throw 'Close Phantom Dust first.' }

if ($Create) {
    if (-not (Test-Path $shim)) { throw "Build pdnet first: $shim" }
    New-Build Direct direct $true
    New-Build Teredo teredo $false
}

if ($Use) {
    $current = Get-Registered
    if ($current -ne $Use) {
        if (Get-AppxPackage $package) {
            Remove-AppxPackage (Get-AppxPackage $package).PackageFullName -PreserveApplicationData
        }
        Add-AppxPackage -Register "$($folders[$Use])\AppxManifest.xml"
    }
}

Write-Host "Registered build: $(Get-Registered)"
