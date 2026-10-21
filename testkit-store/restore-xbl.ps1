# Puts Phantom Dust back on Xbox Live: removes every pdnet change and checks that Teredo (which Xbox Live multiplayer
# uses) can run. For the Microsoft Store install and unpacked copies, set up with this kit or an older one.
#   Double-click Restore-XboxLive.bat (or: powershell -ExecutionPolicy Bypass -File restore-xbl.ps1 [-NoPause])
# What it does:
#   1. Windows no longer starts pdlaunch with the game (package debugger setting).
#   2. Unpacked copy changed by an older kit (Install.bat): the original Microsoft.Xbox.Services.dll and
#      AppxManifest.xml come back, pdnet's files leave the game folder, the game is registered again.
#   3. With administrator permission (one prompt): removes the local-connection exemption, makes sure the IP Helper
#      service (Teredo) runs and the Xbox Live networking service isn't disabled, and turns Teredo back on if it is
#      disabled. A working Teredo setup (including one set by Group Policy) is left as it is.
# Your local pdnet saves (LocalState\pdnet\saves) stay where they are; Xbox cloud saves were never touched.
param([switch]$NoPause)
$ErrorActionPreference = 'Stop'
$kit = $PSScriptRoot
$package = 'Microsoft.MSEsper'
function Done { if (-not $NoPause) { Read-Host 'Press Enter to close' | Out-Null } }
function Fail($msg) { Write-Host "`nERROR: $msg" -ForegroundColor Red; Done; exit 1 }

if (Get-Process PDUWP -ErrorAction SilentlyContinue) { Fail 'Close Phantom Dust first.' }
$pkg = Get-AppxPackage $package
if (-not $pkg) { Write-Host 'Phantom Dust is not installed; only the Windows network check runs.' }
else {
    $store = $pkg.SignatureKind -eq 'Store'
    $folder = $pkg.InstallLocation
    Write-Host "Found Phantom Dust ($(if ($store) { 'Microsoft Store' } else { 'unpacked copy' })) at $folder"

    # --- 1. stop loading pdnet -------------------------------------------------------------------
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
[ComImport, Guid("F27C3930-8029-4AD1-94E3-3DBA417810C1"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IPackageDebugSettingsR
{
    [PreserveSig] int EnableDebugging([MarshalAs(UnmanagedType.LPWStr)] string packageFullName, [MarshalAs(UnmanagedType.LPWStr)] string debuggerCommandLine, IntPtr environment);
    [PreserveSig] int DisableDebugging([MarshalAs(UnmanagedType.LPWStr)] string packageFullName);
}
[ComImport, Guid("B1AEC16F-2383-4852-B0E9-8F0B1DC66B4D")] public class PackageDebugSettingsR { }
public static class PdPlmRestore
{
    public static int Disable(string pkg) { return ((IPackageDebugSettingsR)new PackageDebugSettingsR()).DisableDebugging(pkg); }
}
'@
    $hr = [PdPlmRestore]::Disable($pkg.PackageFullName)
    Write-Host ($(if ($hr -eq 0) { 'pdlaunch no longer starts with the game.' } else { 'pdlaunch was not set up (nothing to undo).' }))

    # --- 2. unpacked copy: files an older kit replaced -----------------------------------------
    if (-not $store) {
        $backup = "$folder\_pdnet_backup"
        $orig = "$folder\Microsoft.Xbox.Services.orig.dll"
        $changed = $false
        if (Test-Path $backup) {
            Copy-Item "$backup\Microsoft.Xbox.Services.dll" "$folder\Microsoft.Xbox.Services.dll" -Force
            if (Test-Path "$backup\AppxManifest.xml") { Copy-Item "$backup\AppxManifest.xml" "$folder\AppxManifest.xml" -Force }
            Remove-Item $backup -Recurse -Force
            $changed = $true
        }
        elseif ((Test-Path $orig) -and (Get-Item "$folder\Microsoft.Xbox.Services.dll").Length -lt 6MB) {
            Copy-Item $orig "$folder\Microsoft.Xbox.Services.dll" -Force
            $changed = $true
        }
        foreach ($f in $orig, "$folder\EOSSDK-Win64-Shipping.dll", "$folder\eos.ini", "$folder\pdnet.ini") {
            if (Test-Path $f) { Remove-Item $f -Force; $changed = $true }
        }
        if ($changed) {
            Write-Host 'Original game files restored in the game folder.'
            if (Test-Path "$kit\nosuspend.ps1") { & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$kit\nosuspend.ps1" -Off | Out-Null }
            Remove-AppxPackage $pkg.PackageFullName -PreserveApplicationData
            try { Add-AppxPackage -Register "$folder\AppxManifest.xml" }
            catch { Fail "Windows refused to register the game again: $($_.Exception.Message)" }
            Write-Host 'Game registered again with its original files.'
        }
        else { Write-Host 'The game folder has no pdnet files.' }
    }
}

# --- 3. Windows network settings (administrator) -----------------------------------------------
$family = 'Microsoft.MSEsper_8wekyb3d8bbwe'
$admin = @"
CheckNetIsolation.exe LoopbackExempt -d -n=$family | Out-Null
Set-Service iphlpsvc -StartupType Automatic; Start-Service iphlpsvc -ErrorAction SilentlyContinue
`$xbl = Get-Service XboxNetApiSvc -ErrorAction SilentlyContinue
if (`$xbl -and `$xbl.StartType -eq 'Disabled') { Set-Service XboxNetApiSvc -StartupType Manual }
if ((netsh interface teredo show state | Out-String) -match '(?m)Type\s*:\s*disabled') { netsh interface teredo set state type=default | Out-Null }
"@
Write-Host "`nWindows will now ask for administrator permission, to remove pdnet's network exemption and check Teredo."
try {
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($admin)) # survives the elevation hop intact
    $p = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -WindowStyle Hidden -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encoded
    if ($p.ExitCode -ne 0) { throw "exit code $($p.ExitCode)" }
}
catch { Write-Host "Warning: the administrator step didn't run ($_). Run Restore-XboxLive.bat again and allow it." -ForegroundColor Yellow }

# --- report ------------------------------------------------------------------------------------
$teredo = (netsh interface teredo show state | Out-String)
$type = if ($teredo -match '(?m)Type\s*:\s*(\S+)') { $Matches[1] } else { '?' }
$state = if ($teredo -match '(?m)State\s*:\s*(\S+)') { $Matches[1] } else { '?' }
$ip = Get-Service iphlpsvc -ErrorAction SilentlyContinue
Write-Host "`nTeredo: type $type, state $state. IP Helper service: $($ip.Status)."
if ($state -notin 'qualified', 'dormant') {
    Write-Host 'Teredo is not qualified yet. Open the Xbox app > Settings > Network (or Xbox Networking in Windows Settings) and use "Fix it".' -ForegroundColor Yellow
}
Write-Host "`nDone. Phantom Dust starts with Xbox Live again." -ForegroundColor Green
if ($pkg -and $pkg.SignatureKind -ne 'Store') { Write-Host 'Note: an unpacked copy may not sign in to Xbox Live or load cloud saves; the Store version does.' }
Write-Host 'You can delete the pdnet kit folder now.'
Done
