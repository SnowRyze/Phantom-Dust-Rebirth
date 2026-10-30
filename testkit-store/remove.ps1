# Undoes setup.ps1: Phantom Dust starts without pdnet again. The game's files were never changed.
#   Double-click Remove.bat (or: powershell -ExecutionPolicy Bypass -File remove.ps1)
$ErrorActionPreference = 'Stop'
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
public static class PdPlmOff
{
    public static int Disable(string pkg) { return ((IPackageDebugSettings)new PackageDebugSettings()).DisableDebugging(pkg); }
}
'@
$pkg = Get-AppxPackage Microsoft.MSEsper
if (-not $pkg) { Write-Host 'Phantom Dust is not installed; nothing to undo.'; Read-Host 'Press Enter to close'; exit }
if (Get-Process PDUWP -ErrorAction SilentlyContinue) { Write-Host 'Close Phantom Dust first.' -ForegroundColor Red; Read-Host 'Press Enter to close'; exit 1 }
$hr = [PdPlmOff]::Disable($pkg.PackageFullName)
if ($hr -ne 0) { Write-Host ('Windows reported 0x{0:X8}.' -f $hr) -ForegroundColor Yellow }
Write-Host 'Windows will ask for administrator permission to remove the local-connection exemption and the clock task Setup.bat added.'
$admin = "Start-Process CheckNetIsolation.exe -Wait -WindowStyle Hidden -ArgumentList 'LoopbackExempt -d -n=$($pkg.PackageFamilyName)'; " +
         "Unregister-ScheduledTask -TaskName 'Phantom Dust clock' -Confirm:`$false -ErrorAction SilentlyContinue"
try { Start-Process powershell.exe -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList "-NoProfile -ExecutionPolicy Bypass -EncodedCommand $([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($admin)))" }
catch { Write-Host "Skipped ($_). It is harmless to leave." }
# Microphone permission: back to what it was before Setup.bat allowed it.
$before = Join-Path $PSScriptRoot 'mic-permission-before.txt'
if (Test-Path $before) {
    $appConsent = "HKCU:\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone\$($pkg.PackageFamilyName)"
    $old = (Get-Content $before -Raw).Trim()
    if ($old -eq 'none') { Remove-ItemProperty $appConsent -Name Value -ErrorAction SilentlyContinue }
    else { Set-ItemProperty $appConsent -Name Value -Value $old -Type String }
    Remove-Item $before
    Write-Host 'Microphone permission for Phantom Dust put back as it was.'
}
if ($pkg.SignatureKind -eq 'Store') { Write-Host 'Done. Phantom Dust starts normally again (Xbox Live, cloud saves). You can delete the kit folder.' }
else { Write-Host 'Done. pdnet no longer loads. Note: an unpacked copy without pdnet may hang on "retrieving profile" (Xbox cloud saves).' }
Write-Host 'Your pdnet test saves stay in the game''s local data (pdnet\saves) and are not used by the normal game.'
Read-Host 'Press Enter to close'
