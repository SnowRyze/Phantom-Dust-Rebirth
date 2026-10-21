# Sets up pdnet for Phantom Dust: the Microsoft Store install or an unpacked copy. The game's files are NOT changed:
# Windows starts pdlaunch.exe (from this folder) every time Phantom Dust starts, and pdlaunch loads pdnet into the game.
#   Double-click Setup.bat (or: powershell -ExecutionPolicy Bypass -File setup.ps1 -Name "MyName" [-GameFolder "D:\Games\PhantomDust"])
# Undo with Remove.bat (remove.ps1). Keep this folder where it is while pdnet is set up.
# An unpacked copy set up with an older kit (which replaced Microsoft.Xbox.Services.dll) gets its original files back first.
param([string]$Name, [string]$GameFolder, [switch]$NoPause)
$ErrorActionPreference = 'Stop'
$kit = $PSScriptRoot
$package = 'Microsoft.MSEsper'
$testedVersion = '1.3.25854.2'
function Done { if (-not $NoPause) { Read-Host 'Press Enter to close' | Out-Null } }
function Fail($msg) { Write-Host "`nERROR: $msg" -ForegroundColor Red; Done; exit 1 }

foreach ($f in 'pdlaunch.exe', 'pdnet.dll', 'EOSSDK-Win64-Shipping.dll', 'pdnet.ini') {
    if (-not (Test-Path "$kit\$f")) { Fail "The kit is incomplete: $f is missing. Unzip the whole kit and run this from that folder." }
}
if (-not (Test-Path "$kit\eos.ini")) { Fail 'eos.ini is missing. The person who sent you the kit sends it separately; put it in this folder.' }
$creds = Get-Content "$kit\eos.ini" | Where-Object { $_ -match '^(ProductId|SandboxId|DeploymentId|ClientId|ClientSecret)=(.+)$' }
if (@($creds).Count -lt 5) { Fail 'eos.ini is incomplete (it needs ProductId, SandboxId, DeploymentId, ClientId and ClientSecret).' }
if (Get-Process PDUWP -ErrorAction SilentlyContinue) { Fail 'Close Phantom Dust first.' }

function Register($folder) {
    $dev = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock' -ErrorAction SilentlyContinue
    if ($dev.AllowDevelopmentWithoutDevLicense -ne 1) { Fail 'Developer Mode is off. Turn it on in Settings > (Update & Security >) For developers, then run this again.' }
    if (-not (Get-AppxPackage Microsoft.VCLibs.140.00 | Where-Object Architecture -eq 'X64')) {
        Fail 'Missing Microsoft.VCLibs.140.00 (x64). Install Microsoft''s "C++ Runtime framework packages for Desktop Bridge", then run this again.'
    }
    try { Add-AppxPackage -Register "$folder\AppxManifest.xml" }
    catch { Fail "Windows refused to install the game from $folder : $($_.Exception.Message)" }
}

# --- find the game --------------------------------------------------------------------------------
$pkg = Get-AppxPackage $package
if (-not $pkg) {
    Write-Host 'Phantom Dust is not installed. If you have the Microsoft Store version, install it from the Store first and run this again.'
    if (-not $GameFolder) { $GameFolder = Read-Host 'If you have an unpacked copy instead, enter its folder (the one with PDUWP.exe and AppxManifest.xml)' }
    $GameFolder = $GameFolder.Trim('"').TrimEnd('\')
    if (-not $GameFolder -or -not (Test-Path "$GameFolder\AppxManifest.xml") -or -not (Test-Path "$GameFolder\PDUWP.exe")) { Fail "No PDUWP.exe / AppxManifest.xml in '$GameFolder'." }
    $head = [IO.File]::ReadAllBytes("$GameFolder\PDUWP.exe")[0..1]
    if ($head[0] -ne 0x4D -or $head[1] -ne 0x5A) { Fail 'PDUWP.exe is not readable as a normal program (still encrypted?). pdnet needs an unpacked copy or the Store install.' }
    Register $GameFolder
    $pkg = Get-AppxPackage $package
}
$store = $pkg.SignatureKind -eq 'Store'
$folder = $pkg.InstallLocation
Write-Host "Found Phantom Dust $($pkg.Version) ($(if ($store) { 'Microsoft Store' } else { 'unpacked copy' })) at $folder"
if ($pkg.Version -ne $testedVersion) { Write-Host "Note: pdnet was built against version $testedVersion. Trying anyway." -ForegroundColor Yellow }

# --- unpacked copy set up by an older kit: put the original files back ----------------------------
if (-not $store) {
    $backup = "$folder\_pdnet_backup"
    $orig = "$folder\Microsoft.Xbox.Services.orig.dll"
    $changed = $false
    if (Test-Path $backup) {
        Write-Host 'This copy was set up with an older pdnet kit (replaced files). Restoring the original files...'
        Copy-Item "$backup\Microsoft.Xbox.Services.dll" "$folder\Microsoft.Xbox.Services.dll" -Force
        if (Test-Path "$backup\AppxManifest.xml") { Copy-Item "$backup\AppxManifest.xml" "$folder\AppxManifest.xml" -Force }
        Remove-Item $orig, "$folder\EOSSDK-Win64-Shipping.dll", "$folder\eos.ini", "$folder\pdnet.ini" -ErrorAction SilentlyContinue
        Remove-Item $backup -Recurse -Force
        $changed = $true
    }
    elseif ((Test-Path $orig) -and (Get-Item "$folder\Microsoft.Xbox.Services.dll").Length -lt 6MB) {
        Write-Host 'Microsoft.Xbox.Services.dll in this copy is a pdnet build; putting the original back...'
        Copy-Item $orig "$folder\Microsoft.Xbox.Services.dll" -Force
        Remove-Item $orig
        $changed = $true
    }
    if ($changed) {
        # In its own process: it defines the same Windows interface type as this script does below.
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $kit 'nosuspend.ps1') -Off | Out-Null
        Remove-AppxPackage $pkg.PackageFullName -PreserveApplicationData
        Register $folder
        $pkg = Get-AppxPackage $package
        Write-Host 'Original files restored.'
    }
}

# --- name -----------------------------------------------------------------------------------------
if (-not $Name) { $Name = Read-Host 'Your player name (what the other players will see, max 32 characters)' }
$Name = ($Name -replace '[\r\n=$]', '').Trim()
if (-not $Name) { $Name = $env:COMPUTERNAME }
if ($Name.Length -gt 32) { $Name = $Name.Substring(0, 32) }
$ini = Get-Content "$kit\pdnet.ini" -Raw
Set-Content "$kit\pdnet.ini" ($ini -replace '(?m)^display_name=.*$', "display_name=$Name") -Encoding UTF8 -NoNewline

# --- let the game load pdnet from this folder -----------------------------------------------------
# The game runs in a sandbox; let it read (only) this folder, so it can load pdnet.dll and the settings files.
Get-ChildItem $kit -Recurse -File | Unblock-File
& icacls.exe $kit /grant '*S-1-15-2-1:(OI)(CI)(RX)' /T /Q | Out-Null
if ($LASTEXITCODE -ne 0) { Fail 'Could not give the game read access to this folder (icacls failed). Move the folder somewhere under your user folder and try again.' }

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
[ComImport, Guid("F27C3930-8029-4AD1-94E3-3DBA417810C1"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IPackageDebugSettings
{
    [PreserveSig] int EnableDebugging([MarshalAs(UnmanagedType.LPWStr)] string packageFullName, [MarshalAs(UnmanagedType.LPWStr)] string debuggerCommandLine, IntPtr environment);
    [PreserveSig] int DisableDebugging([MarshalAs(UnmanagedType.LPWStr)] string packageFullName);
}
[ComImport, Guid("B1AEC16F-2383-4852-B0E9-8F0B1DC66B4D")] public class PackageDebugSettings { }
public static class PdPlm
{
    public static int Enable(string pkg, string cmd) { return ((IPackageDebugSettings)new PackageDebugSettings()).EnableDebugging(pkg, cmd, IntPtr.Zero); }
}
'@
$hr = [PdPlm]::Enable($pkg.PackageFullName, "`"$kit\pdlaunch.exe`"")
if ($hr -ne 0) { Fail ('Windows refused to attach pdlaunch to Phantom Dust (HRESULT 0x{0:X8}).' -f $hr) }

# pdnet passes match traffic between the game and itself through sockets on this PC, which Windows blocks for
# Store-style apps unless they are exempted. This needs administrator rights once.
Write-Host "`nWindows will now ask for administrator permission, to let Phantom Dust use connections on this PC (needed for matches)."
try {
    $p = Start-Process CheckNetIsolation.exe -Verb RunAs -Wait -PassThru -WindowStyle Hidden -ArgumentList "LoopbackExempt -a -n=$($pkg.PackageFamilyName)"
    if ($p.ExitCode -ne 0) { throw "exit code $($p.ExitCode)" }
    Write-Host 'Done.'
}
catch { Write-Host "Warning: the local-connection exemption was not set ($_). Lobbies will work, but matches may not connect; run Setup.bat again and allow it." -ForegroundColor Yellow }

Write-Host "`nSet up as '$Name'. Start Phantom Dust the normal way (Start menu)." -ForegroundColor Green
Write-Host 'pdnet now loads every time the game starts, and the game keeps running when minimized.'
Write-Host "Check after the first start: $kit\pdlaunch.log should say 'pdnet loaded into the game'."
Write-Host 'To stop using pdnet, double-click Remove.bat.'
Done
