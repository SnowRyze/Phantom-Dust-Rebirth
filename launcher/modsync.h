#pragma once
// Mod loader step 2 (Conceptual; docs\MOD_LOADER_CODE.md): pdlaunch keeps PDPL's mods folder in step with the player's
// mod list before PDPL is loaded. PDPL mounts every archive in the root of RoamingState\mods, in name order (first wins),
// at game start. So the enabled mods are copied there as "!rb000-<id>.zip", "!rb001-..." in priority order ("!" sorts
// before letters and digits, so they come before the player's own archives), and the player's own archives that are
// turned off wait in RoamingState\mods\disabled\. PDPL's 1.2.2 DLL exports nothing, so pdnet can't look inside archives
// through it; pdlaunch runs outside the game's sandbox and reads them with Windows' tar.exe.
#include <string>

namespace modsync
{
    using LogFn = void (*)(const wchar_t* fmt, ...);

    // Mods come from (in this order of precedence for ids):
    //   LocalState\pdnet\modstore\<id>\*.zip|.7z  catalog downloads (the newest file of each id)
    //   LocalState\pdnet\mods\*.zip|.7z           the player's mods for Rebirth's menu
    //   RoamingState\mods\*.zip|.7z (not !rb*)    the player's own PDPL mods; RoamingState\mods\disabled\ the ones turned off
    // settings.ini: mods_order=id,... (top wins), mods_enabled=id,... (a mod not in mods_order yet is on; without
    // mods_enabled every mod is on). Catalog and Local mods go first in mods_order's order; the player's own PDPL archives
    // follow in name order (PDPL's), as their names decide that.
    // Writes LocalState\pdnet\mods-state.txt (the result, for pdnet's Mods menu) and caches what's inside each archive in
    // LocalState\pdnet\modinfo\. Never throws; problems are logged and listed in the state file.
    void Sync(const std::wstring& localState, LogFn log);
}
