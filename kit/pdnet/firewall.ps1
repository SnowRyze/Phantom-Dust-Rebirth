# The administrator step of Setup.bat and Remove.bat / Restore-XboxLive.bat (runs elevated, without a window).
#   -Add     lets Phantom Dust use connections on this PC: the loopback exemption (CheckNetIsolation), Windows
#            Firewall allow rules for the app (group "pdnet", UDP in and out), and any firewall rule that BLOCKS the app
#            switched off (Windows makes one when its firewall question was cancelled; a block rule wins over any allow).
#            The names of the rules switched off are kept in firewall-disabled.txt, so -Remove can switch them on again.
#   -Remove  undoes all of that.
# pdnet's game packets go from the game to pdnet's links over this PC's own address. A player whose firewall blocked
# that (2026-11-10) could join no lobby: the links connected, but the game's packets never reached them.
# What it did is written to firewall.log next to this script (the window is hidden).
param([switch]$Add, [switch]$Remove, [string]$Family = 'Microsoft.MSEsper_8wekyb3d8bbwe')
$ErrorActionPreference = 'Continue'
$kit = $PSScriptRoot
$logFile = Join-Path $kit 'firewall.log'
$disabledFile = Join-Path $kit 'firewall-disabled.txt'
function Note([string]$text) { Add-Content $logFile "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $text" }

# The app's security id (an AppContainer SID), which firewall rules for a Store-style app are made for.
Add-Type -Namespace PdFw -Name Native -MemberDefinition @'
[DllImport("userenv.dll", CharSet = CharSet.Unicode)] public static extern int DeriveAppContainerSidFromAppContainerName(string name, out IntPtr sid);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern bool ConvertSidToStringSid(IntPtr sid, out string text);
[DllImport("advapi32.dll")] public static extern IntPtr FreeSid(IntPtr sid);
'@
$sid = $null
$ptr = [IntPtr]::Zero
if ([PdFw.Native]::DeriveAppContainerSidFromAppContainerName($Family.ToLower(), [ref]$ptr) -eq 0) {
    $text = $null
    if ([PdFw.Native]::ConvertSidToStringSid($ptr, [ref]$text)) { $sid = $text }
    [PdFw.Native]::FreeSid($ptr) | Out-Null
}

# The firewall rules that name this app (by its SID, or by a PDUWP.exe program path).
function AppRules {
    $filters = Get-NetFirewallApplicationFilter -All -ErrorAction SilentlyContinue | Where-Object {
        ($sid -and $_.Package -eq $sid) -or ($_.Program -and $_.Program -like '*\PDUWP.exe')
    }
    foreach ($f in $filters) { $f | Get-NetFirewallRule -ErrorAction SilentlyContinue }
}

if ($Add) {
    Note "add: $Family, app SID $sid"
    CheckNetIsolation.exe LoopbackExempt -a -n=$Family | Out-Null
    $loopback = $LASTEXITCODE
    Note "loopback exemption: exit $loopback"

    $off = @()
    foreach ($rule in (AppRules | Where-Object { $_.Action -eq 'Block' -and $_.Enabled -eq 'True' -and $_.Group -ne 'pdnet' })) {
        Disable-NetFirewallRule -Name $rule.Name -ErrorAction SilentlyContinue
        $off += $rule.Name
        Note "block rule switched off: '$($rule.DisplayName)' ($($rule.Direction), $($rule.Name))"
    }
    if ($off) { Add-Content $disabledFile $off }

    Get-NetFirewallRule -Group 'pdnet' -ErrorAction SilentlyContinue | Remove-NetFirewallRule -ErrorAction SilentlyContinue
    if ($sid) {
        foreach ($direction in 'Inbound', 'Outbound') {
            New-NetFirewallRule -DisplayName "Phantom Dust (pdnet) UDP $direction" -Group 'pdnet' -Direction $direction -Action Allow `
                -Protocol UDP -Package $sid -Profile Any -ErrorAction SilentlyContinue | Out-Null
            Note "allow rule ($direction): $(if ($?) { 'added' } else { 'FAILED' })"
        }
    }
    else { Note 'no app SID - allow rules not added' }
    exit $loopback
}

if ($Remove) {
    Note "remove: $Family"
    CheckNetIsolation.exe LoopbackExempt -d -n=$Family | Out-Null
    Get-NetFirewallRule -Group 'pdnet' -ErrorAction SilentlyContinue | Remove-NetFirewallRule -ErrorAction SilentlyContinue
    Note 'allow rules removed'
    if (Test-Path $disabledFile) {
        foreach ($name in (Get-Content $disabledFile | Where-Object { $_ })) {
            Enable-NetFirewallRule -Name $name -ErrorAction SilentlyContinue
            Note "block rule switched on again: $name"
        }
        Remove-Item $disabledFile -ErrorAction SilentlyContinue
    }
    exit 0
}
