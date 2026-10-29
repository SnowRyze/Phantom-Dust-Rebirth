# Sets up pdnet for Phantom Dust: the Microsoft Store install or an unpacked copy. The game's files are NOT changed:
# Windows starts pdlaunch.exe (from this folder) every time Phantom Dust starts, and pdlaunch loads pdnet into the game.
#   Double-click Setup.bat (or: powershell -ExecutionPolicy Bypass -File setup.ps1 -Name "MyName" [-GameFolder "D:\Games\PhantomDust"])
# This script and everything it uses are in the kit's pdnet folder; Setup.bat (the only file outside it) runs it.
# Undo with pdnet\Remove.bat (remove.ps1). Keep the kit folder where it is while pdnet is set up.
# An unpacked copy set up with an older kit (which replaced Microsoft.Xbox.Services.dll) gets its original files back first.
#   -Mic yes|no answers the microphone question without asking (voice chat).
param([string]$Name, [string]$GameFolder, [ValidateSet('', 'yes', 'no')][string]$Mic = '', [switch]$NoPause)
$ErrorActionPreference = 'Stop'
$kit = $PSScriptRoot
$package = 'Microsoft.MSEsper'
$testedVersion = '1.3.25854.2'
# Asks a question after dropping keypresses already waiting: the Enter that opened Setup.bat could otherwise answer
# the first question with nothing (Windows Terminal passes it on).
function Ask([string]$question) {
    try { $Host.UI.RawUI.FlushInputBuffer() } catch {}
    Read-Host $question
}
function Done { if (-not $NoPause) { Ask 'Press Enter to close' | Out-Null } }
function Fail($msg) { Write-Host "`nERROR: $msg" -ForegroundColor Red; Done; exit 1 }

foreach ($f in 'pdlaunch.exe', 'pdnet.dll', 'EOSSDK-Win64-Shipping.dll', 'pdnet.ini') {
    if (-not (Test-Path "$kit\$f")) { Fail "The kit is incomplete: $f is missing from $kit. Unzip the whole kit (Setup.bat and the pdnet folder) and run Setup.bat again." }
}
if (-not (Test-Path "$kit\eos.ini")) { Fail "eos.ini is missing. The person who sent you the kit sends it separately; put it in $kit." }
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
    if (-not $GameFolder) { $GameFolder = Ask 'If you have an unpacked copy instead, enter its folder (the one with PDUWP.exe and AppxManifest.xml)' }
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
# The name used last time is offered, so pressing Enter (or setting up a newer kit) keeps it: remembered in
# %LOCALAPPDATA%\pdnet-name.txt, else the name pdnet last signed in with (its log in the game's data).
$nameFile = Join-Path $env:LOCALAPPDATA 'pdnet-name.txt'
$previous = if (Test-Path $nameFile) { (Get-Content $nameFile -Raw -Encoding UTF8).Trim() } else { '' }
if (-not $previous) {
    $log = Join-Path $env:LOCALAPPDATA "Packages\$($pkg.PackageFamilyName)\LocalState\pdnet.log"
    if (Test-Path $log) {
        $signedIn = Select-String -Path $log -Pattern "eos: signed in as '(.+?)'" -Encoding UTF8 | Select-Object -Last 1
        if ($signedIn) { $previous = $signedIn.Matches[0].Groups[1].Value }
    }
}
if ($previous -eq $env:COMPUTERNAME) { $previous = '' } # an earlier empty answer, not a chosen name
$prompt = 'Your player name (what the other players will see, max 32 characters)'
if ($previous) { $prompt += " [$previous - press Enter to keep it]" }
for ($try = 0; -not $Name -and $try -lt 3; ++$try) {
    $Name = ((Ask $prompt) -replace '[\r\n=$]', '').Trim()
    if (-not $Name -and $previous) { $Name = $previous }
    elseif (-not $Name) { Write-Host 'Please type a name.' -ForegroundColor Yellow }
}
$Name = ($Name -replace '[\r\n=$]', '').Trim()
if (-not $Name) { $Name = $env:COMPUTERNAME } # three empty answers with no earlier name
if ($Name.Length -gt 32) { $Name = $Name.Substring(0, 32) }
$ini = Get-Content "$kit\pdnet.ini" -Raw -Encoding UTF8 # UTF-8: voice_talk_mark is a music note
Set-Content "$kit\pdnet.ini" ($ini -replace '(?m)^display_name=.*$', "display_name=$Name") -Encoding UTF8 -NoNewline
Set-Content $nameFile $Name -Encoding UTF8 -ErrorAction SilentlyContinue

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

# Two things need administrator rights, asked for once:
# - pdnet passes match traffic between the game and itself through sockets on this PC, which Windows blocks for
#   Store-style apps unless they are exempted.
# - The scheduled task "Phantom Dust clock": PCs whose clocks are a day or more apart can't connect in a match, so
#   pdlaunch starts this task when the clock is an hour or more off. It runs as SYSTEM (which may set the clock) and
#   sets it from the Date header of Epic's server, over HTTPS: the certificate is checked as usual, except that one
#   outside its dates is accepted (a clock that far off makes every certificate look expired or not yet valid). The
#   script is written to "C:\Program Files\Phantom Dust pdnet\clock.ps1", which only administrators can change (a
#   file in the kit folder could be swapped by anyone, and would then run as SYSTEM). It can't be an -EncodedCommand
#   in the task: Windows never starts an encoded command that makes a web request (2026-10-29, the task hung).
#   Every signed-in user may start the task (read + execute in its security descriptor). Remove.bat deletes both.
$clockFix = @'
Add-Type -TypeDefinition 'using System.Net; using System.Net.Security; using System.Security.Cryptography.X509Certificates; public static class PdClockCert { public static bool Check(object s, X509Certificate c, X509Chain chain, SslPolicyErrors e) { if (e == SslPolicyErrors.None) return true; if (e != SslPolicyErrors.RemoteCertificateChainErrors) return false; foreach (var st in chain.ChainStatus) if (st.Status != X509ChainStatusFlags.NotTimeValid) return false; return true; } public static void Use() { ServicePointManager.ServerCertificateValidationCallback = Check; } }'
[PdClockCert]::Use()
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$r = [Net.HttpWebRequest]::Create('https://api.epicgames.dev/'); $r.Proxy = $null; $r.Method = 'HEAD'; $r.AllowAutoRedirect = $false; $r.Timeout = 5000
try { $resp = $r.GetResponse() } catch [Net.WebException] { $resp = $_.Exception.Response }
$epic = [DateTime]::Parse($resp.Headers['Date']).ToUniversalTime()
if ([Math]::Abs(([DateTime]::UtcNow - $epic).TotalSeconds) -gt 60) { Set-Date -Date $epic.ToLocalTime() | Out-Null }
'@
$admin = @"
`$exempt = (Start-Process CheckNetIsolation.exe -Wait -PassThru -WindowStyle Hidden -ArgumentList 'LoopbackExempt -a -n=$($pkg.PackageFamilyName)').ExitCode
try {
    `$dir = Join-Path `$env:ProgramFiles 'Phantom Dust pdnet'
    New-Item -ItemType Directory -Force `$dir -ErrorAction Stop | Out-Null
    [IO.File]::WriteAllText("`$dir\clock.ps1", [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('$([Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($clockFix)))')))
    `$ps = "`$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
    `$action = New-ScheduledTaskAction -Execute `$ps -Argument "-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File ```"`$dir\clock.ps1```""
    `$principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
    `$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Minutes 2)
    Register-ScheduledTask -TaskName 'Phantom Dust clock' -Action `$action -Principal `$principal -Settings `$settings -Force -ErrorAction Stop | Out-Null
    `$service = New-Object -ComObject Schedule.Service; `$service.Connect()
    `$service.GetFolder('\').GetTask('Phantom Dust clock').SetSecurityDescriptor('D:(A;;FA;;;BA)(A;;FA;;;SY)(A;;GRGX;;;AU)', 0)
    `$clock = 0
} catch { `$clock = 1 }
exit (`$exempt -bor (`$clock -shl 8))
"@
Write-Host "`nWindows will now ask for administrator permission, to let Phantom Dust use connections on this PC (needed for matches)"
Write-Host "and to let it set this PC's clock right when it is off (players whose clocks are far apart can't connect)."
try {
    $p = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -WindowStyle Hidden -ArgumentList "-NoProfile -ExecutionPolicy Bypass -EncodedCommand $([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($admin)))"
    if ($p.ExitCode -band 0xff) { Write-Host "Warning: the local-connection exemption was not set (exit code $($p.ExitCode -band 0xff)). Lobbies will work, but matches may not connect; run Setup.bat again and allow it." -ForegroundColor Yellow }
    if ($p.ExitCode -shr 8) { Write-Host 'Warning: the clock task was not set up; a wrong clock will not be fixed at game start.' -ForegroundColor Yellow }
    if (-not $p.ExitCode) { Write-Host 'Done.' }
}
catch { Write-Host "Warning: administrator permission was not given ($_). Lobbies will work, but matches may not connect; run Setup.bat again and allow it." -ForegroundColor Yellow }

# --- microphone (voice chat) ----------------------------------------------------------------------
# Windows asks each Store-style app once before it may use the microphone (desktop programs, like games from other
# launchers, only need the global switch). Answering here sets that per-app permission up front, for this Windows user
# only. The previous value is kept in mic-permission-before.txt so Remove.bat can put it back.
$consent = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone'
$appConsent = "$consent\$($pkg.PackageFamilyName)"
$current = (Get-ItemProperty $appConsent -ErrorAction SilentlyContinue).Value
$voiceOff = (Get-Content "$kit\pdnet.ini" -Encoding UTF8) -match '^\s*voice_chat\s*=\s*off\s*$'
if ($voiceOff) { } # this kit has voice chat switched off: the microphone isn't used, nothing to ask
elseif ($current -eq 'Allow') { Write-Host "`nMicrophone: Phantom Dust may already use it (voice chat)." }
else {
    if (-not $Mic -and -not $NoPause) {
        $answer = Ask "`nAllow Phantom Dust to use your microphone for voice chat? You can mute yourself in the lobby (Options > Mic). (Y/N)"
        $Mic = if ($answer -match '^\s*y') { 'yes' } else { 'no' }
    }
    if ($Mic -eq 'yes') {
        if (-not (Test-Path "$kit\mic-permission-before.txt")) { Set-Content "$kit\mic-permission-before.txt" $(if ($current) { $current } else { 'none' }) }
        if (-not (Test-Path $appConsent)) { New-Item $appConsent -Force | Out-Null } # -Force on an existing key would wipe its values
        Set-ItemProperty $appConsent -Name Value -Value 'Allow' -Type String
        Write-Host 'Microphone allowed for Phantom Dust.'
    }
    elseif ($Mic -eq 'no') { Write-Host 'Microphone not changed: Windows will ask the first time voice chat uses it (you can still hear others).' }
}
if ((Get-ItemProperty $consent -ErrorAction SilentlyContinue).Value -eq 'Deny') {
    Write-Host 'Note: microphone access for apps is switched off in Windows (Settings > Privacy > Microphone), so others will not hear you.' -ForegroundColor Yellow
}

Write-Host "`nSet up as '$Name'. Start Phantom Dust the normal way (Start menu)." -ForegroundColor Green
Write-Host 'pdnet now loads every time the game starts, and the game keeps running when minimized.'
Write-Host "Check after the first start: $kit\pdlaunch.log should say 'pdnet loaded into the game'."
Write-Host "To stop using pdnet, double-click Remove.bat in $kit."
Done
