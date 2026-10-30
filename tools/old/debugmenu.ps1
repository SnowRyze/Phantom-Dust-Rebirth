# Moves the debug menu from Main Menu > Options to Main Menu > Multiplayer > Split Screen.
#   .\debugmenu.ps1          apply   (Options = real options, Split Screen = debug menu)
#   .\debugmenu.ps1 -Revert  undo    (back to how the dump shipped)
#
# How it works: the game loads Assets\Data\Menu\<name>_en.ssb for each menu (name table in PDUWP.exe).
# The dump had the debug menu copied over option_en.ssb, with the real options kept as option_enx.ssb.
# Split Screen opens vsmode_en.ssb, so the debug menu goes there instead.
param([switch]$Revert)
$ErrorActionPreference = 'Stop'
$menu = Resolve-Path "$PSScriptRoot\..\Assets\Data\Menu"
$backup = "$PSScriptRoot\original\menu"

New-Item -ItemType Directory -Force $backup | Out-Null
foreach ($f in 'option_en.ssb', 'vsmode_en.ssb') {
    if (-not (Test-Path "$backup\$f")) { Copy-Item "$menu\$f" "$backup\$f" }   # as shipped in the dump
}

if ($Revert) {
    Copy-Item "$backup\option_en.ssb" "$menu\option_en.ssb" -Force
    Copy-Item "$backup\vsmode_en.ssb" "$menu\vsmode_en.ssb" -Force
    Write-Host 'Reverted: Options = debug menu, Split Screen = normal.'
    return
}

Copy-Item "$menu\option_enx.ssb" "$menu\option_en.ssb" -Force          # real options menu
Copy-Item "$backup\option_en.ssb" "$menu\vsmode_en.ssb" -Force         # the dump's (patched) debug menu
Write-Host 'Applied: Options = real options, Multiplayer > Split Screen = debug menu.'
