#pragma once
// Mod loader step 2 (Conceptual; docs\MOD_LOADER_CODE.md): pdlaunch keeps PDPL's mods folder in step with the player's
// mod list before PDPL is loaded. PDPL mounts every archive in the root of RoamingState\mods, in name order (first wins),
// at game start. So the enabled mods are copied there as "!rb000-<id>.zip", "!rb001-..." in priority order ("!" sorts
// before letters and digits, so they come before the player's own archives), and the player's own archives that are
// turned off wait in RoamingState\mods\disabled\. PDPL's 1.2.2 DLL exports nothing, so pdnet can't look inside archives
// through it; pdlaunch runs outside the game's sandbox and reads them: .zip with Windows' tar.exe, .7z with 7-Zip when
// it's installed (tar.exe can't read .7z). An archive it can't read still goes to PDPL, just without its details.
#include <functional>
#include <string>

namespace modsync
{
    using LogFn = void (*)(const wchar_t* fmt, ...);

    // pdlaunch's HTTPS and hashing (the updater's), for the catalog and the downloads.
    struct Net
    {
        bool (*get)(const std::wstring& url, std::string& body, unsigned long timeoutMs); // HTTPS GET; false unless 200
        std::string (*sha256)(const std::string& data);                                  // lowercase hex
        // The tag of a GitHub repo's newest release ("owner/repo"), from the website's redirect, not the API (which allows
        // 60 lookups an hour without a login, "not modified" answers included); as it appears in the URL (percent-encoded).
        bool (*latestTag)(const std::wstring& repo, std::string& tag, unsigned long timeoutMs);
    };

    // Mods come from (in this order of precedence for ids):
    //   LocalState\pdnet\modstore\<id>\*.zip|.7z  catalog downloads (the newest file of each id)
    //   LocalState\pdnet\mods\*.zip|.7z           the player's mods for Rebirth's menu
    //   RoamingState\mods\*.zip|.7z (not !rb*)    the player's own PDPL mods; RoamingState\mods\disabled\ the ones turned off
    // settings.ini: mods_order=id,... (top wins), mods_enabled=id,... (a mod not in mods_order yet is on; without
    // mods_enabled every mod is on). Catalog and Local mods go first in mods_order's order; the player's own PDPL archives
    // follow in name order (PDPL's), as their names decide that.
    // Writes LocalState\pdnet\mods-state.txt (the result, for pdnet's Mods menu) and caches what's inside each archive in
    // LocalState\pdnet\modinfo\. Never throws; problems are logged and listed in the state file.
    //
    // The catalog (step 5): catalogUrl (https://..., or a local file for testing) lists the mods Rebirth can download,
    // one per line: "<id> <github owner/repo> [plugin]" (# comments). It's fetched at every start (the copy in
    // modstore\catalog.txt is used when it can't be reached). Catalog mods are off until the player turns them on in the
    // Mods list; for each one that's on, the newest release's tag is read at every start from GitHub's website (free:
    // github.com/<repo>/releases/latest redirects to .../tag/<tag>), and only a tag not seen before is looked up in
    // GitHub's API (60 lookups an hour; results kept in modstore\releases.txt). Its first .zip / .7z is downloaded to
    // modstore\<id>\<tag>.<ext> and checked
    // against GitHub's own sha256 of it (or a "sha256 <file> <hex>" line in the release notes, which wins); the newest 3
    // of each mod are kept. Catalog mods that aren't
    // downloaded are listed in the state file too, without a path. net null: no catalog, no downloads.
    // gameDir: the game folder (for new map slots: the game's stage list and MNet.alr, mapslots.h); empty: no map slots.
    // beforeFiles (live mod loading, while the game runs): called after the downloads and before any file the game could
    // use is touched, with "" or why these changes can't be made while the game runs (something changes that the game or
    // PDPL only reads at start: plugins, skill data, text, shaders, unknown files; BootOnly). False from it: nothing is
    // changed and Sync returns false (a lobby's mods are put back for the next start).
    bool Sync(const std::wstring& localState, LogFn log, const Net* net, const std::wstring& catalogUrl, const std::wstring& gameDir,
              const std::function<bool(const std::string& needsRestart)>& beforeFiles = {});
}
