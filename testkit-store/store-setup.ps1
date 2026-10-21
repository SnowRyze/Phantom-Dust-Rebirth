# Sets up the pdnet test for the Microsoft Store install of Phantom Dust. The game's files are NOT changed.
# Windows will start pdlaunch.exe (from this folder) every time Phantom Dust starts, and pdlaunch loads pdnet into the game.
#   Double-click Setup.bat (or: powershell -ExecutionPolicy Bypass -File store-setup.ps1 -Name "MyName")
# Undo with Remove.bat (store-remove.ps1). Keep this folder where it is while the test is set up.
param([string]$Name)
$ErrorActionPreference = 'Stop'
$kit = $PSScriptRoot
function Fail($msg) { Write-Host "`nERROR: $msg" -ForegroundColor Red; Read-Host 'Press Enter to close'; exit 1 }

foreach ($f in 'pdlaunch.exe', 'pdnet.dll', 'EOSSDK-Win64-Shipping.dll', 'pdnet.ini') {
    if (-not (Test-Path "$kit\$f")) { Fail "The kit is incomplete: $f is missing. Unzip the whole kit and run this from that folder." }
}
if (-not (Test-Path "$kit\eos.ini")) { Fail 'eos.ini is missing. The person who sent you the kit sends it separately; put it in this folder.' }
$creds = Get-Content "$kit\eos.ini" | Where-Object { $_ -match '^(ProductId|SandboxId|DeploymentId|ClientId|ClientSecret)=(.+)$' }
if (@($creds).Count -lt 5) { Fail 'eos.ini is incomplete (it needs ProductId, SandboxId, DeploymentId, ClientId and ClientSecret).' }

$pkg = Get-AppxPackage Microsoft.MSEsper
if (-not $pkg) { Fail 'Phantom Dust is not installed. Install it from the Microsoft Store first.' }
Write-Host "Found Phantom Dust $($pkg.Version) ($($pkg.SignatureKind) install) at $($pkg.InstallLocation)"
if ($pkg.SignatureKind -ne 'Store') { Fail 'This Phantom Dust is an unpacked copy, not the Microsoft Store install. Use the stage C kit (Install.bat) for unpacked copies.' }
if ($pkg.Version -ne '1.3.25854.2') { Write-Host "Note: pdnet was built against version 1.3.25854.2. Trying anyway." -ForegroundColor Yellow }
if (Get-Process PDUWP -ErrorAction SilentlyContinue) { Fail 'Close Phantom Dust first.' }

if (-not $Name) { $Name = Read-Host 'Your player name (what the other tester will see, max 32 characters)' }
$Name = ($Name -replace '[\r\n=$]', '').Trim()
if (-not $Name) { $Name = $env:COMPUTERNAME }
if ($Name.Length -gt 32) { $Name = $Name.Substring(0, 32) }
$ini = Get-Content "$kit\pdnet.ini" -Raw
Set-Content "$kit\pdnet.ini" ($ini -replace '(?m)^display_name=.*$', "display_name=$Name") -Encoding UTF8 -NoNewline

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

# The Store game may only talk to the internet. pdnet passes match traffic between the game and itself through sockets
# on this PC, which Windows blocks for Store apps unless they are exempted. This needs administrator rights once.
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
Write-Host 'To play normally again (Xbox Live), double-click Remove.bat.'
Read-Host 'Press Enter to close'
