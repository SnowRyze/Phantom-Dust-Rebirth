# Stops Windows from suspending Phantom Dust when it's minimized or in the background.
# Uses Windows' package debug mode (IPackageDebugSettings, what Visual Studio uses), for this package only.
#   .\nosuspend.ps1         turn it on  (game keeps running when minimized)
#   .\nosuspend.ps1 -Off    turn it off (normal Windows behaviour)
# It applies to whichever Phantom Dust build is registered (they share one package identity). If a build switch
# turns it off again, just run this once more.
param([switch]$Off)
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

[ComImport, Guid("F27C3930-8029-4AD1-94E3-3DBA417810C1"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IPackageDebugSettings
{
    [PreserveSig] int EnableDebugging([MarshalAs(UnmanagedType.LPWStr)] string packageFullName,
                                      [MarshalAs(UnmanagedType.LPWStr)] string debuggerCommandLine, IntPtr environment);
    [PreserveSig] int DisableDebugging([MarshalAs(UnmanagedType.LPWStr)] string packageFullName);
}

[ComImport, Guid("B1AEC16F-2383-4852-B0E9-8F0B1DC66B4D")]
public class PackageDebugSettings { }

public static class Plm
{
    public static int Enable(string pkg)  { return ((IPackageDebugSettings)new PackageDebugSettings()).EnableDebugging(pkg, null, IntPtr.Zero); }
    public static int Disable(string pkg) { return ((IPackageDebugSettings)new PackageDebugSettings()).DisableDebugging(pkg); }
}
'@

$pkg = Get-AppxPackage Microsoft.MSEsper
if (-not $pkg) { throw 'Phantom Dust is not registered. Register a build first (builds.ps1 -Use ...).' }

$hr = if ($Off) { [Plm]::Disable($pkg.PackageFullName) } else { [Plm]::Enable($pkg.PackageFullName) }
if ($hr -ne 0) { throw ('Windows refused (HRESULT 0x{0:X8}).' -f $hr) }

if ($Off) { Write-Host 'Suspension back to normal: Windows will pause Phantom Dust when it is minimized.' }
else { Write-Host "Phantom Dust ($($pkg.InstallLocation)) will keep running when minimized or in the background." }
Write-Host 'Takes effect the next time the game starts.'
