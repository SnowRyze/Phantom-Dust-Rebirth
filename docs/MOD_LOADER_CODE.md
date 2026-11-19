# Mod loader: code plan (PDPL)

What code gets written for Rebirth's mod menu, file by file and step by step. The why is in `docs/CONCEPTUAL.md`
("Mod loader and mod menu"). Conceptual branch only. Written 2026-11-19; no code exists yet. Addresses are PDUWP.exe's
(image base 0x140000000), as in the rest of pdnet.

**Decision (the user, 2026-11-19): Torphedo's PDPL loads the mod files.** Rebirth ships `pd_loader_core.dll` in its kit,
pdlaunch injects it while the game is still suspended, and pdnet decides which mods are mounted, in what order, through
PDPL's PhysicsFS functions. Rebirth adds everything around it: the in-game Mods menu, the catalog and downloads, auto
sort, lobbies, skill data in memory. PDPL is used unchanged; fixes go to Torphedo (section 12), not into a copy.

PDPL source read: `Torphedo/pd_loader_core` commit `4264ea4` (2025-11-26). Newest release: **1.2.2** (2025-12-24,
asset `pdpl.zip`, sha256 `62a043e7...543e`), built from that same commit.

**1.2.2's `pd_loader_core.dll` has no export table** (an MSVC build; checked with dumpbin, 2026-11-19). So pdnet can't
call its PhysicsFS functions: the live remount (sections 5.2-5.3, step 3) waits on a PDPL build that exports them
(request 1). Until then mods change at the next start only (the `!rbNN-` links, or `modlist.txt` if PDPL adds it).

**Step 2 as built (2026-11-19): in pdlaunch, not pdnet** (`launcher/modsync.cpp`). Without PDPL exports pdnet can't look
inside archives, but pdlaunch runs outside the game's sandbox before PDPL loads, so it does sections 5.4-5.5 and 6.2-6.5
itself at every start: scans the three folders, lists each archive and reads its `pdmod.json` with Windows' `tar.exe`
(cached in `LocalState\pdnet\modinfo\`), applies `mods_order` / `mods_enabled`, copies the enabled Catalog / Local
mods into PDPL's folder as `!rb000-<id>.zip` ... (copies, not hard links: a copy gets the folder's permissions, so a mod
the player moved instead of copied still works; a copy is only made again when the source's size or time changes),
moves the player's own archives to and from `disabled\`, and writes `LocalState\pdnet\mods-state.txt` (every mod in
priority order with its details, plus `conflict` lines) for pdnet's Mods menu (step 4). `pdlaunch.exe -syncmods
<LocalState>` runs only this, for testing. pdnet writes `mods-boot.txt` when PDPL loads and deletes it past the title
screen (safe mode), and logs the mod list.

**Torphedo's answers (2026-11-19):**
- The console only appears when the game is sideloaded, and hiding the window would be easy. Rebirth supports unpacked
  (sideloaded) copies, so pdnet handles it itself (section 4.3); no change in PDPL needed.
- PhysicsFS locks each call. The gap is between two calls in PDPL's hook (`PHYSFS_exists` then `PHYSFS_getRealDir`).
- `fake\` copies keyed by file name only: a real problem for modding the motion (exmot) files; they should also be
  deleted at start-up. Agreed to fix.
- A `CreateFileW` hook: fine if needed.
- `modlist.txt` (a mount list): if PDPL adds it, it's the one record of which mods are on and their order. Rebirth's
  Mods menu writes it (no other program), and `mods_enabled` / `mods_order` in settings.ini go away. Turned-off
  mods can stay in it as `# id` lines to keep their place.
- `pdmod.json` needs nothing from PDPL: it's Rebirth's metadata only. `touches` isn't a field: it's worked out
  from the paths at every scan, so it can't be out of date. Dependencies by id (`load_after` / `load_before`) stay.
- Forwarding PDPL's output into Rebirth's log can be done inside pdnet: step 1 does it (section 4.3).

Words used here:
- **game path**: a file path as the game opens it (`Assets/Data/player/pc01a/pc01a2.alr`, `3dsprite_ps.cso`).
- **mod**: one `.zip` or `.7z` (PDPL reads both), with an optional `pdmod.json` inside.
- **catalog**: the curated list of mods (`mods/mods.txt` in this repo).
- **mounted set**: the archives in PDPL's search path, in priority order (first wins).
- **PDPL's mods folder**: `%LOCALAPPDATA%\Packages\<package>\RoamingState\mods`.

---

## 0. Overview

```
pdlaunch (game suspended)
  ├─ inject pdnet.dll  (as today)
  ├─ inject pd_loader_core.dll from the kit's pdnet\ folder   (skipped: mods=off, or safe mode)
  ├─ wait until KERNELBASE!CreateFile2 in the game starts with a jump (PDPL's hook is in)
  └─ resume the game

PDPL (its own thread)          hooks CreateFile2, holds its mutex (game file opens wait), mounts:
                                 mods folder (loose files) > archives in its root, by name > game folder,
                               loads /plugins/*.dll, releases the mutex

pdnet
  mods.cpp     catalog + downloads (https.cpp) + manifests + enabled set + order + auto sort + lobby tags
               keeps PDPL's mods folder in step:  !rb00-<id>.zip, !rb01-<id>.zip ... (hard links)  -> next start
  pdpl.cpp     PhysicsFS functions from pd_loader_core.dll: remount in the new order  -> now, no restart
  gamepatches  Options > Mods (list, toggle, move, auto sort), SESSION INFO "(Unrated, 3 mods)",
               a mod's skill data into the block at 0x1404c5240
  xsapi/mpsd   PD_MODS lobby attribute, join check, modded lobbies forced Unrated
```

Two ways the mounted set changes, both driven by the same ordered list:
- **Next start:** pdnet keeps PDPL's mods folder holding exactly the enabled mods, named so that PDPL's alphabetical
  order is the menu order. PDPL mounts them itself at start, early enough for shaders and plugins.
- **Now:** pdnet unmounts and remounts through PhysicsFS, so files the game opens from then on come from the new set.
  Files the game read once at start (shaders, probably audio) need the restart.

### New files
| File | What it is |
|---|---|
| `src/pdpl.h`, `src/pdpl.cpp` | The bridge to `pd_loader_core.dll`: PhysicsFS functions by GetProcAddress, read / set the mounted set, look inside an archive, read a file from the VFS |
| `src/mods.h`, `src/mods.cpp` | Catalog, manifests, enabled set and order, auto sort, PDPL's mods folder, live apply, lobby tags, safe mode |
| `src/https.h`, `src/https.cpp` | WinHTTP GET (redirects, size cap) and SHA-256 |
| `deps/pdpl/` | `pd_loader_core.dll` from PDPL's release, `VERSION.txt` (version, source commit, sha256) |
| `mods/mods.txt` | The catalog |
| `mods/README.md` | How to make and publish a mod (for creators) |
| `tools/make-mod.ps1` | Checks `pdmod.json`, zips a mod folder, prints the release-notes `sha256` line |

### Changed files
| File | Change |
|---|---|
| `launcher/pdlaunch.cpp` | `InjectPdpl(pid)` after `Inject(pid)`, with the hook wait; skipped for `mods=off` and safe mode |
| `src/dllmain.cpp` | `mods::Start()` in `EnsureInitialized`; on the non-injected (Linux) path, load `pd_loader_core.dll` from pdnet's folder first |
| `src/config.h`, `src/config.cpp` | New keys (section 3) |
| `src/gamepatches.h`, `src/gamepatches.cpp` | Options > Mods, the mods list (menu 68 in a third mode), SESSION INFO text, skill data from mods, exports `InMatch` / `MatchLoading` / `ApplyModSkillData`, title reached → `mods::TitleReached()` |
| `xsapi/mpsd.h`, `xsapi/mpsd.cpp` | `SessionDoc::mods`, PD_MODS, join check, Unrated for modded lobbies, search-result tags |
| `tools/build.ps1` | Checks `deps\pdpl\pd_loader_core.dll` against VERSION.txt's sha256 and copies it into the kit |
| `kit/pdnet/README.txt`, `README.md` | Credit for PDPL; feature line once it works |

Nothing in pdnet hooks file opens for mods. pdnet's existing import hook on PDUWP's CreateFile2 (title art, debug
menu) stays: it calls KERNELBASE!CreateFile2, which is PDPL's hook, so both run (title art first, then PDPL).

---

## 1. Folders and file formats

| Path | Who writes it | What |
|---|---|---|
| kit `pdnet\pd_loader_core.dll` | the kit | PDPL. The kit folder is readable by app packages (setup.ps1 grants `S-1-15-2-1` RX), as pdnet.dll is |
| `RoamingState\mods\` | PDPL creates it; pdnet keeps it in step | PDPL's mods folder. Loose files here win over every archive (PDPL mounts the folder first) |
| `RoamingState\mods\!rbNN-<id>.zip` / `.7z` | pdnet | Hard links to enabled mods, NN = 00, 01, ... in menu order. `!` sorts before letters and digits, so these come before the player's own archives |
| `RoamingState\mods\<anything else>.zip` / `.7z` | the player | Their own PDPL mods. Listed in the menu as "PDPL folder" |
| `RoamingState\mods\disabled\` | pdnet | The player's own archives they turned off in the menu are moved here (PDPL only mounts archives in the folder's root) |
| `RoamingState\mods\plugins\` | the player | Loose plugin DLLs; PDPL loads them. Not managed by pdnet |
| `LocalState\pdnet\mods\` | the player | Mods for Rebirth's menu, placed by hand (`.zip`, `.7z`). Listed as "Local" |
| `LocalState\pdnet\modstore\<id>\<tag>.zip` | pdnet | Downloaded catalog mods; newest 3 tags of each kept |
| `LocalState\pdnet\modstore\catalog.txt`, `releases.txt` | pdnet | Cached catalog and release lookups |
| `LocalState\pdnet\modinfo.txt` | pdnet | Cached inspection results (manifest + file list summary), keyed by path, size and write time |
| `LocalState\pdnet\inspect\` | pdnet | Temporary hard links used to look inside an archive (section 5.5) |
| `LocalState\pdnet\mods-boot.txt` | pdnet | Exists from start until the title screen is passed. pdlaunch finding it = the last start died: no PDPL this time |
| `RoamingState\fake\` | PDPL | PDPL's copies of files read from archives |

LocalState and RoamingState are both in the package folder, so hard links between them work. If `CreateHardLinkW`
fails, pdnet copies instead (logged).

### `mods/mods.txt` (the catalog)
```
# id            repo                     flags
skillpack-toaf  toafguy/pd-skillpack
neon-arena      someone/pd-neon-arena
freecam         someone/pd-freecam       plugin
```
- `id`: `[a-z0-9-]{3,32}`, fixed forever once listed.
- `repo`: `owner/name` on GitHub.
- `plugin`: this mod may contain DLLs under `plugins/` (added only after someone has read its source). A catalog mod
  without the flag that ships DLLs is refused ("contains plugins but isn't marked as a plugin in the catalog").
- Fetched from `mod_list_url` (default
  `https://raw.githubusercontent.com/SnowRyze/Phantom-Dust-Rebirth/Conceptual/mods/mods.txt`), at most once per 6 hours.

### `pdmod.json` (optional, at the archive's root)
```json
{
  "id": "neon-arena",
  "name": "Neon Arena",
  "version": "1.2.0",
  "author": "someone",
  "description": "Stage 3 at night, with neon lights.",
  "online": "match",
  "load_after": ["skillpack-toaf"],
  "load_before": []
}
```
| Field | Default | Meaning |
|---|---|---|
| `id` | file name, cleaned | Must equal the catalog id for catalog mods |
| `name`, `version`, `author`, `description` | file name, "", "", "" | Shown in the menu |
| `online` | `match` | `match`: changes the game for everyone, must match the host. `local`: only looks or sounds different on this PC |
| (`touches`) | not a field | Worked out from the paths at every scan: `skills maps models animation audio shaders ui text plugins other` |
| `load_after`, `load_before` | none | Ids. `load_after: [X]` = my files win over X's |

PDPL ignores `pdmod.json` (it's just another file in the archive), so the same archive works with plain PDPL too.
Suggest the format to Torphedo so both loaders read it.

### Release notes line
`sha256 <asset name> <64 hex>` (as pdnet's own releases). The asset is the release's first `.zip` or `.7z`.

### Lobby attribute `PD_MODS`
`id@tag,id@tag,...` in priority order: only the host's enabled catalog mods with `online: match`, through
`lobbies::Split`. Absent = vanilla lobby.

---

## 2. Step plan

Each step is one Conceptual kit, tried in the game before the next.

| Step | Kit content | Done when |
|---|---|---|
| 1 | PDPL bundled, injected by pdlaunch before resume | A test archive in PDPL's mods folder changes a model, a map and a shader; no console window problems, no busy thread (section 4.3) |
| 2 | `pdpl.cpp`, `mods.cpp`: scan, inspect, `mods_enabled` / `mods_order`, PDPL's mods folder kept in step | Turning a mod off in settings.ini takes it out at the next start; order decides which of two mods wins |
| 3 | Live apply (remount) + safe mode | A model changes with no restart after an apply from the main menu; a start killed on the logos skips PDPL next time |
| 4 | The Mods menu | Toggle, move, auto sort work with pad and keyboard; leaving applies |
| 5 | Catalog + downloads | A catalog mod downloads, its sha256 is checked, it shows in the menu |
| 6 | Skill data from mods | A mod's gsdata takes effect live and goes back to vanilla when turned off |
| 7 | Lobbies | The browser shows a lobby's mods; a joiner without them gets them and joins; Unrated |
| 8 | Plugins from the catalog | A catalog plugin mod loads at the next start; lobbies ask for a restart for one |

---

## 3. Settings (`config.h` / `config.cpp`)

```cpp
bool mods = true;                 // mods=on|off: off = pdlaunch doesn't load PDPL, the menu says so
std::string modsEnabled;          // mods_enabled=id,id,...  (the Mods menu writes it)
std::string modsOrder;            // mods_order=id,id,...    every known mod, top = wins
std::wstring lobbyMods = L"auto"; // lobby_mods=auto|off: joining a modded lobby downloads and uses its mods; off: the join fails
std::wstring modListUrl;          // mod_list_url=: the catalog (empty: the default)
```
`Load` parses them as the others; the Mods menu saves with `SaveSettings({{"mods_enabled", ...}, {"mods_order", ...}})`.
pdlaunch reads `mods` with its existing `Setting()` (settings.ini over pdnet.ini).

---

## 4. pdlaunch: loading PDPL

### 4.1 `InjectPdpl(DWORD pid, const std::wstring& localState)`
Called right after `Inject(pid)` succeeds and before the game is resumed.
1. `Setting(L"mods") == L"off"` → log `mods: off - PDPL not loaded` and return.
2. `localState + L"\\pdnet\\mods-boot.txt"` exists → log
   `mods: the last start didn't reach the title screen - starting without mods (PDPL not loaded)`, and return. pdnet sees
   PDPL missing and shows safe mode in the menu.
3. `g_dir + L"pd_loader_core.dll"` missing → log and return.
4. `RunRemote(process, LoadLibraryW, remotePath, 30000, L"LoadLibraryW(pd_loader_core.dll)")`, the same code as
   `Inject` (factor the remote-LoadLibrary part of `Inject` into `RemoteLoadLibrary(process, path, what)` and use it
   for both).
5. Wait for PDPL's hook: PDPL's DllMain only starts a thread, and that thread hooks CreateFile2 and takes its mutex.
   Until then the game could open files PDPL never sees. Poll up to 5 s, every 1 ms:
   `ReadProcessMemory(process, CreateFile2 address, buf, 1)`, done when `buf[0] == 0xE9` (MinHook's jump). KERNELBASE
   is at the same address in every process of a boot, so the address comes from pdlaunch's own
   `GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "CreateFile2")`. Log
   `mods: PDPL loaded (file hook in after <n> ms)`, or `... file hook not seen after 5 s - resuming anyway`.
6. Once the hook is in, PDPL's mutex makes the game's file opens wait until PDPL has mounted its mods and loaded its
   plugins, so the game can be resumed straight away.

### 4.2 Linux (no pdlaunch)
pdnet is the game's `Microsoft.Xbox.Services.dll` there. `EnsureInitialized` (non-injected path) calls
`LoadLibraryW(<pdnet's folder>\pd_loader_core.dll)` before anything else. That's late (after the start-up logos), so
files the game opened before it come from vanilla. `build-linux.sh` / `setup-linux.sh` copy the DLL next to pdnet.

### 4.3 PDPL's console and output (done in step 1, `src/pdpl.cpp`)
From PDPL's code (`dll_main.c`, `console.c`): it calls `AllocConsole` and shows the window (sideloaded copies), and
its command loop `while (running) { fgets(...); command_exec(line); }` would get end-of-file at once, every time, where
there's no console (Store copies), and may spin a core. pdnet deals with both without a change in PDPL:
- pdlaunch runs pdnet's export `PdBeforePdpl` in the game right before it loads `pd_loader_core.dll`.
- `pdpl::BeforeLoad` makes a console of pdnet's own and hides it at once, so PDPL's `AllocConsole` fails (one console
  per process): no window, and PDPL never reopens its output to a console.
- It points standard input at an empty pipe (PDPL's `fgets` waits there for good), and standard output and error at a
  pipe. PDPL's C runtime takes those handles when the DLL loads. A pdnet thread copies the text, without PDPL's colour
  codes and with a time on each line, to `LocalState\pdnet\pdpl.log` (the start before is kept as `pdpl.prev.log`);
  PDPL's own messages (not its `CreateFile2` lines) also go to pdnet.log, the first 200. Options > Send Log uploads
  pdpl.log too.
- The output is a pipe, so PDPL's C runtime buffers it: lines arrive in blocks of about 4 KB.
- `pdpl::Report` (pdnet's set-up) logs whether PDPL is in the game, and after 1 and 5 minutes the CPU time of the
  threads that started in `pd_loader_core.dll`.

---

## 5. `src/pdpl.h` / `src/pdpl.cpp`: the bridge

### 5.1 Interface
```cpp
namespace pdnet::pdpl
{
    // pd_loader_core.dll is in the game and its PhysicsFS functions were found. Waits up to timeoutMs for PDPL to finish
    // its own start-up (PHYSFS_isInit, and the game folder in the search path, which PDPL mounts last).
    bool Ready(DWORD timeoutMs = 0);
    std::string Version(); // from the DLL's file version, or "unknown"

    // The search path as PhysicsFS has it (exact strings, as mounted), first = wins.
    std::vector<std::string> SearchPath();
    std::string ModsFolder();  // PDPL's mods folder entry (the first one PDPL mounts)
    std::string GameFolder();  // the game folder entry (the last one PDPL mounts)

    // Makes the archives between the mods folder and the game folder exactly `archives`, in that order: unmounts the
    // archives there now and the game folder, then appends `archives` and the game folder again. Retries an unmount that
    // fails with files still open (10 x 20 ms). Returns false (logged) if anything failed; what it could do stays done.
    bool SetArchives(const std::vector<std::string>& archives);

    // Looks inside an archive that isn't mounted: mounts it at /.rebirth/inspect, lists every file, reads pdmod.json,
    // unmounts it. False if PhysicsFS can't open it.
    struct Contents { std::vector<std::string> files; std::string manifest; };
    bool Inspect(const std::string& archive, Contents& out);

    // A file through the VFS as the game would get it: which archive or folder it comes from, and its bytes.
    std::string WhereFrom(const std::string& gamePath);            // PHYSFS_getRealDir, "" if nowhere
    bool ReadFile(const std::string& gamePath, std::vector<uint8_t>& out, size_t maxBytes);
}
```

### 5.2 Loading the functions
`GetModuleHandleW(L"pd_loader_core.dll")`, then `GetProcAddress` for: `PHYSFS_isInit`, `PHYSFS_mount`,
`PHYSFS_unmount`, `PHYSFS_getSearchPath`, `PHYSFS_freeList`, `PHYSFS_enumerateFiles`, `PHYSFS_stat`,
`PHYSFS_getRealDir`, `PHYSFS_exists`, `PHYSFS_openRead`, `PHYSFS_fileLength`, `PHYSFS_readBytes`, `PHYSFS_close`,
`PHYSFS_getLastErrorCode`, `PHYSFS_getErrorByCode`. pd_loader_core is built with MinGW and declares no exports of its
own, so the linker exports every symbol, PhysicsFS's included (PDPL's readme has plugins link them). All found →
`Ready`; any missing → log which, and mods still work at the next start (the mods folder), just never live.

Function types are declared locally (`using MountFn = int (*)(const char*, const char*, int);` ...); no PhysicsFS
header is needed. PhysicsFS is thread-safe for these calls (its state lock), so pdnet calls them from its worker.

### 5.3 `SetArchives`
```
path = SearchPath()
mods = path.front(), game = path.back()            (checked: mods ends in "\mods", game is the game folder)
for each entry between them: PHYSFS_unmount(entry)  (with retries)
PHYSFS_unmount(game)
for each archive in order: PHYSFS_mount(archive, "/", 1)
PHYSFS_mount(game, "/", 1)
```
While this runs, an open by the game finds the file missing from the VFS and PDPL opens the original path: vanilla for
a moment, never a failure. `mods::Apply` only calls it outside matches and match loading. Remaining race: PDPL's hook
checks `PHYSFS_exists` and then `PHYSFS_getRealDir` without holding PhysicsFS' lock in between, so an unmount between
the two gives it a null directory (upstream request 2).

Archive strings are built the way PDPL builds its own, `<mods folder entry> + "\" + <file name>`, so an archive mounted
at start by PDPL and the same archive remounted by pdnet have the same string.

### 5.4 Why `!rbNN-` names and hard links (instead of mounting pdnet's store directly)
PDPL mounts the archives in its mods folder at start. If pdnet mounted `modstore\...` itself, that would happen after
PDPL's start-up: too late for shaders and for PDPL's plugin loading. With the links, the next start's order is already
the menu's order before the game runs. pdnet's live remount uses the same link paths, so both ways give the same set.

### 5.5 `Inspect`
PhysicsFS treats a second mount of an already-mounted path as done (it compares the path string), so an archive that is
mounted at `/` can't also be mounted at `/.rebirth/inspect`. pdnet inspects a different path to the same file:
- catalog mods: the `modstore\<id>\<tag>.zip` original (the link in PDPL's mods folder is a different string);
- Local mods: the original in `LocalState\pdnet\mods\`;
- the player's own archives in PDPL's mods folder: a temporary hard link `LocalState\pdnet\inspect\<n>.<ext>`,
  deleted afterwards.
Results are cached in `modinfo.txt` (path, size, write time → id, name, version, touches, file count, paths of
interest), so this only happens for new or changed archives. Game paths never start with `/.rebirth`, so the brief
mount can't change what the game gets.

---

## 6. `src/mods.h` / `src/mods.cpp`: the mod manager

### 6.1 Interface
```cpp
namespace pdnet::mods
{
    enum class Source { Catalog, Local, PdplFolder };

    struct Mod
    {
        std::string id, name, version, author, description, tag; // tag: release tag (catalog)
        Source source = Source::Local;
        bool onlineMatch = true;            // online: match
        bool hasPlugins = false;            // has plugins/*.dll
        bool pluginAllowed = false;         // catalog flag
        std::vector<std::string> touches, loadAfter, loadBefore;
        std::wstring file;                  // the archive ("" = catalog mod not downloaded)
        std::string sha256;                 // catalog: checked against the release notes
        bool enabled = false;
        bool movable = true;                // PDPL folder archives: fixed at the bottom, in name order
        bool needsRestart = false;          // changed this session and touches something read at start
        std::string problem;                // shown in the menu
    };

    void Start();                     // worker thread (section 6.2)
    void TitleReached();              // deletes mods-boot.txt
    bool SafeMode();                  // PDPL wasn't loaded because the last start died
    bool PdplLoaded();

    std::vector<Mod> List();          // menu order
    void SetEnabled(const std::string& id, bool on);
    void Move(const std::string& id, int step);
    void AutoSort();
    bool Apply(std::string* why);     // save, sync PDPL's mods folder, remount; false during a match
    std::string ConflictText(const std::string& id);

    std::string LobbyTag();
    bool SameAsActive(const std::string& tag);
    bool PrepareForLobby(const std::string& tag, DWORD timeoutMs, std::string* why);
    void Prefetch(const std::string& tag);
    void LeftLobby();
    std::vector<std::string> TagNames(const std::string& tag);
    void SetSearchResults(std::vector<std::string> tags);
    std::string SearchResultTag(size_t index);
}
```

### 6.2 `Start` (worker thread)
1. `mods=off` → log, return. Write `mods-boot.txt` (pdlaunch reads it at the next start; `TitleReached` deletes it).
2. `PdplLoaded()` = `GetModuleHandleW(L"pd_loader_core.dll") != nullptr`. Not loaded and the file
   `mods-boot.txt` existed at start → safe mode.
3. `pdpl::Ready(10000)`.
4. Scan: `LocalState\pdnet\mods\` (Local), `modstore\` (Catalog, newest tag per id), PDPL's mods folder and its
   `disabled\` (PdplFolder; `!rb*` files are pdnet's own links and are skipped). Inspect new or changed archives.
5. Catalog: `modstore\catalog.txt`; refetched if older than 6 h.
6. Order and enabled from settings.ini. A mod that's in PDPL's mods folder root counts as enabled (it is mounted).
7. Compare what PDPL mounted at start with the wanted set. They differ only if settings.ini was edited by hand or a
   sync failed last session: `Apply()` then puts it right (logged).
8. Log one line per mounted archive: `mods: 1: neon-arena 1.2.0 (catalog, match, 214 files)`.

### 6.3 `Apply`
Refused while `gamepatches::InMatch()` or `gamepatches::MatchLoading()` (the Mods menu isn't reachable from a lobby, so a
host can't change mods mid-lobby). Then:
1. `SaveSettings` (`mods_enabled`, `mods_order`).
2. Unmount every managed archive (`pdpl::SetArchives` with only the player's archives that stay where they are): an
   archive PhysicsFS has open can't be renamed or deleted.
3. Sync PDPL's mods folder: delete `!rb*` links not wanted; create or rename the wanted ones to `!rbNN-<id>.<ext>` in
   menu order (`CreateHardLinkW(link, original)`); move disabled player archives to `disabled\`, enabled ones back.
4. `pdpl::SetArchives(links in order, then the player's enabled archives by name)`.
5. `needsRestart` for each changed mod whose `touches` include something the game reads only at start (the list from
   step 1; until then: `shaders`, `audio`, `plugins`).
6. `gamepatches::ApplyModSkillData()`.
7. Log `mods: applied: a, b, c (restart needed for: b)`.

### 6.4 Auto sort
Order index 0 = top = wins. Only movable mods are sorted; PDPL folder archives stay at the bottom in name order.
1. Tier (lower = higher in the list): cosmetics (`touches` only `ui audio shaders`, or `online: local`) 0; models /
   animation 1; maps 2; skills / text 3; plugins and `other` 4.
2. `A.load_after` contains B → A above B; `A.load_before` contains B → A below B.
3. Kahn's algorithm; among the free mods the lowest (tier, name) goes first.
4. A cycle keeps its current relative order and each mod in it gets `problem = "load order loop with X"`.

### 6.5 Conflicts
From the cached file lists: for each pair of enabled mods, the number of game paths both have. The menu's hint for a
mod: "Wins over Neon Arena for 12 files" / "Neon Arena wins for 3 files". Loose game files in PDPL's mods folder win
over all archives; if there are any, the menu's first hint line says so.

### 6.6 Lobbies
- `LobbyTag()`: enabled catalog mods with `onlineMatch`, in order, `id@tag`.
- `PrepareForLobby(tag, ...)`: refuse if `lobby_mods=off`, a match is running, an id isn't in the catalog, or a mod in
  the tag has plugins and isn't already mounted ("needs a restart (plugin)"). Download what's missing at the exact tag
  (section 7). Then the active set = the tag's mods in its order, plus this PC's enabled `online: local` mods on top.
  Local and PDPL-folder mods that aren't `online: local` are unmounted for the lobby (their effect on a match is
  unknown). `Apply` without saving to settings.ini, so the player's own choice isn't lost.
- `LeftLobby()`: `Apply` the saved settings again.
- `Prefetch(tag)`: starts the downloads on a worker; SESSION INFO calls it for the lobby under the cursor.

### 6.7 Safe mode
pdnet writes `mods-boot.txt` at start; `TitleReached()` (called from gamepatches' `SignalPastTitle()`, where
past-title.txt is written) deletes it. If the game dies before the title screen, pdlaunch skips PDPL at the next start.
The Mods menu then says "Started without mods: the last start didn't reach the title screen. They're back at the next
start." The next start loads PDPL as normal.

---

## 7. `src/https.*` and GitHub

### 7.1 `https.h`
```cpp
namespace pdnet::https
{
    DWORD Get(const std::wstring& url, std::string& body, DWORD timeoutMs, size_t maxBytes = 64u << 20,
              const std::wstring& extraHeaders = L""); // HTTP status, 0 = not reached
    bool GetToFile(const std::wstring& url, const std::wstring& path, DWORD timeoutMs, size_t maxBytes);
    std::string Sha256Hex(const void* data, size_t size);
    std::string Sha256HexFile(const std::wstring& path);
}
```
Written fresh from discord.cpp's `Request` (same WinHTTP calls), with redirects followed (GitHub's asset downloads
redirect) and `#pragma comment(lib, "winhttp.lib")`. discord.cpp is left alone.

### 7.2 Release lookups (mods.cpp)
- Newest: `GET https://api.github.com/repos/<repo>/releases/latest`; exact tag (lobby joins):
  `GET .../releases/tags/<tag>`. Header `Accept: application/vnd.github+json`.
- From the JSON (`Windows.Data.Json`): `tag_name`, `body` → `sha256 <asset> <hex>`, the first asset ending in `.zip`
  or `.7z`: `browser_download_url`, `size` (refused over 200 MB).
- Cached 6 h in `releases.txt`. GitHub allows 60 unauthenticated requests an hour; a 403 with
  `X-RateLimit-Remaining: 0` uses the cache (logged).
- Download to `modstore\<id>\<tag>.<ext>.part`, hash check, rename. A wrong hash deletes it:
  `mods: <id> <tag>: download doesn't match its sha256 - not used`.

---

## 8. `gamepatches.cpp` changes

### 8.1 List-screen modes
`g_theatre` (bool) becomes `enum class ListMode { Settings, Theatre, Mods }` in `std::atomic<ListMode> g_listMode`,
with `InTheatre()` and `InModsList()`. Every `g_theatre` use (`HookedPushMenu`, `HookedOptionChanged`,
`HookedButtons`, `HookedButtonBar`, `SettingsItems`, `SettingsScriptPatch`) switches on the mode. Settings and Theatre
behave as before (checked in step 4).

### 8.2 Options > Mods (main menu, menu 4)
- A ninth item after Updates in `LogItems`: `g_modsRowText` ("Mods: 3 on" / "Mods: off" / "Mods: safe mode" / "Mods:
  PDPL missing"), hint "Turn mods on and off, and choose which wins when two change the same file."
- `CheckSendLogPick`: event 108 with label `aMAIN_S8` → `OpenModsList()`.
- `OpenModsList()`: `g_listMode = ListMode::Mods`, then as `OpenCaseMenu`: `*At(0x1404bbc34) = 0`,
  `g_pushMenu(manager, kSettingsMenu, 0)`, clear `manager + 0x1062c`. `HookedPushMenu` resets the mode to Settings only
  when 68 is opened from the multiplayer menu.
- Title: `SettingsScriptPatch` relabels "ONLINE SETTINGS" → "MODS".
- Back must return to Options (menu 4); if the script goes to the multiplayer menu instead, `HookedPushMenu` redirects
  36 → 4 while the mode is Mods (checked in step 4).

### 8.3 The mods list
The Theatre's machinery: rows are buffers the window keeps reading.
```cpp
constexpr int kModRows = 10;
std::vector<mods::Mod> g_modList;
char g_modRow[kModRows][112], g_modHint[kModRows][200];
int g_modRows = 0, g_modPage = 0, g_movingMod = -1;
void RefreshModRows(); void FillModPage();
```
- Row: `"\a160w%s %s\f\a1#\a230c%s %s\f\a0#"`: a box (`[x]` / `[ ]`, or two inline icons once `icon_gallery=on` finds
  a tick and an empty box), name, version, and `Online` / `Only you` / `Local` / `PDPL folder`.
- Hint: description, `by <author>`, conflicts, problem, "Restart the game to finish", page.
- Empty: "No mods yet. Put a .zip or .7z in LocalState\pdnet\mods."
- Buttons (`HookedButtons` in Mods mode, once per press as the Settings screen does):
  - **A**: toggle (`mods::SetEnabled`).
  - **X**: pick up / put down the highlighted mod; while it's picked up, up / down move it (`mods::Move`), shown with
    `>` before its name. Not for PDPL folder archives.
  - **Y**: auto sort.
  - **LB / RB** (Page Up / Page Down, Q / E): pages.
  - **B**: leave; changes → `mods::Apply`, the Options row shows the result.
  - Keyboard: Space = A, X = X, Y = Y (`KeysHeld`).
- Button bar `"\a2#Toggle \a?#Move \a?#Auto sort \a580r\a3#Back"`. The pad bits for X / Y in g1 #37's value and their
  icon numbers are unknown: step 4 first logs the value while each face button is held.
- Log: `mods menu: opened (N)`, `... <id> on/off`, `... <id> moved to <n>`, `... auto sort`, `... applied`.

### 8.4 SESSION INFO (lobby browser, menu 43)
The "Game Rule: %s" hook (around gamepatches.cpp:3445) reads `mods::SearchResultTag(index)`. Not empty → the rule
reads `"<rule> (Unrated, 3 mods)"` and the hint line lists `TagNames` with "(you have it)" / "(will download)" /
"(needs a restart)". The lobby under the cursor → `mods::Prefetch(tag)`.

### 8.5 Skill data from mods
PDPL redirects the game's read of `gsdata_<lang>.dat`, but the game then copies its stored online copy over the block
(0x1404c5240, `kSkillDataSize`; flag 0x1403dd465), so a file-only skill mod would be lost. pdnet writes it into memory:
- `ApplyModSkillData()` (worker): `pdpl::WhereFrom("Assets/Data/gstorage/gsdata_<lang>.dat")` (exact case from step 1's
  PDPL output). From the game folder → no mod: put the guard's `stock` back if a mod had been applied. From an
  archive → `pdpl::ReadFile` (exactly `kSkillDataSize` bytes), wait for flag 0x1403dd465, `memcpy` into the block.
- `SkillDataGuard`: the mod's bytes become its `modded` state (so it keeps them, not fights them); log names the source
  ("skill data from mod <id>"). PD Helper packs keep working as now.

### 8.6 Exports (gamepatches.h)
```cpp
bool InMatch();          // existing (state 0x1404bc950), moved out of the anonymous namespace
bool MatchLoading();     // g_matchLoadingSince != 0
void ApplyModSkillData();
```
`SignalPastTitle()` calls `mods::TitleReached()`.

---

## 9. `xsapi/mpsd.*` changes

- `SessionDoc`: `std::string mods;` (PD_MODS).
- `SessionAttributes`: `if (!d.mods.empty()) lobbies::Split(a, "PD_MODS", d.mods);`. `FromLobby`:
  `d.mods = lobbies::Joined(a, "PD_MODS");`.
- Creating (before `lobbies::Create`): `doc.mods = mods::LobbyTag();` and log it.
- Joining (before `lobbies::Join`, non-created branch):
  ```cpp
  if (auto peek = lobbies::Read(lobbyId))
  {
      std::string tag = lobbies::Joined(peek->attributes, "PD_MODS");
      if (!mods::SameAsActive(tag))
      {
          std::string why;
          if (!mods::PrepareForLobby(tag, 45000, &why))
          {
              Log(L"xsapi-pdnet: not joining %S: %S", lobbyId.c_str(), why.c_str());
              status = mp::WriteSessionStatus::HandleNotFound; // the game's "couldn't join"
              return nullptr;
          }
      }
  }
  ```
  An empty tag with match mods mounted → `PrepareForLobby("")` unmounts them for the lobby. Leaving → `mods::LeftLobby()`.
  Open for step 7: how long the game's join waits; `Prefetch` keeps the usual wait short.
- Unrated: where `doc.rated` comes from the search handle (mpsd.cpp:891): `if (!doc.mods.empty()) doc.rated = 0;`.
- Browser: next to `SetSearchResultsRated(...)`, `mods::SetSearchResults(tags)` in result order.
- Member attribute `PD_MODS` (`MemberAttributes`); the owner logs a member whose tag differs.

---

## 10. Plugins

PDPL loads every `.dll` / `.exe` under VFS `/plugins/` at start, from loose files and from mounted archives, and they
can't be unloaded. So:
- A plugin mod turned on or off takes effect at the next start (its link is in PDPL's mods folder by then):
  `needsRestart`.
- Catalog mods with `plugins/` need the catalog's `plugin` flag; without it, pdnet never links them into PDPL's mods
  folder. Local and PDPL folder mods are the player's own choice.
- Lobbies never cause a plugin to load mid-session: a lobby needing a plugin mod that isn't running fails the join with
  "needs a restart (plugin)". (Later: offer restart and rejoin.)

---

## 11. Build and kit

- `deps\pdpl\VERSION.txt`: `1.2.2`, the source commit, and the sha256 of `pd_loader_core.dll`. `build.ps1` checks the
  DLL against it (stops the build on a mismatch) and copies it into `out\kit\pdnet\`. Updating PDPL = replace the DLL,
  edit VERSION.txt.
- The kit's README.txt: "Mods are loaded by PDPL (Phantom Dust Plugin Loader) by Torphedo:
  https://github.com/Torphedo/pd_loader_core".
- Updates: the kit's update zip carries the DLL like any other kit file; pdlaunch replaces it before injecting.
- No C sources, no MinHook, no zip library in pdnet.

---

## 12. Requests for Torphedo (upstream)

Updated after Torphedo's answers (2026-11-19). In order of importance:
1. **Exports.** 1.2.2's `pd_loader_core.dll` has no export table, so nothing outside it can call PhysicsFS. Export the
   `PHYSFS_*` functions (section 5.2's list), or a few of PDPL's own (mount, unmount, search path). Needed for changing
   mods without a restart (step 3). Worth checking whether plugins that import from `pd_loader_core.dll` work with 1.2.2.
2. **`fake\` copies keyed by the full path,** deleted at start-up (Torphedo agreed: needed for the exmot files).
3. **`modlist.txt`:** PDPL mounts exactly the archives listed, in that order (`# id` lines skipped). Rebirth's Mods
   menu writes it; it replaces the `!rbNN-` links and the order in settings.ini.
4. **The remount gap:** one `PHYSFS_getRealDir` call in the hook (its string copied) instead of `PHYSFS_exists` then
   `PHYSFS_getRealDir`, so a remount can't land between the two.
5. **A `CreateFileW` hook too,** if step 1 shows files the game opens through `fopen` that PDPL misses.

The console, the command loop and forwarding PDPL's output are handled in pdnet (section 4.3). Steps 1, 2 and 4-8 don't
wait on any of these; step 3 waits on 1.

---

## 13. Creator tooling

`tools/make-mod.ps1 -Folder D:\MyMod`: checks `pdmod.json` (fields, id format), warns about paths the game
doesn't have (with `-Game <folder>`), zips with `/` names to `<id>-<version>.zip`, prints
`sha256 <name> <hex>` and the catalog line. `mods/README.md`: layout (PDPL's: game paths at the root, `plugins/` for
DLLs), `pdmod.json`, `online: match` vs `local`, publishing a GitHub release, asking for a catalog entry, plugin review.

---

## 14. Logs
```
(pdlaunch) mods: PDPL loaded (file hook in after 3 ms)
mods: PDPL 1.2.2 ready: 4 archives mounted
mods: catalog: 3 mods (cached 2 h ago)
mods: 1: neon-arena 1.2.0 (catalog, match, 214 files)
mods menu: neon-arena off
mods: applied: skillpack-toaf (restart needed for: neon-arena)
xsapi-pdnet: hosting with mods: skillpack-toaf@v2.0
xsapi-pdnet: lobby needs mods neon-arena@v1.2.0 - downloading
mods: neon-arena v1.2.0: downloaded (sha256 ok)
```

## 15. Tests per step
1. Test archive in PDPL's mods folder with a recoloured `pc01a2.alr`, an edited `st03.dat` and an edited `.cso`; each
   shows. Console window: yes / no, Store and unpacked copy. PDPL's thread CPU. pdlaunch.log has the hook line. Title
   art still reads "Rebirth".
2. Two Local mods replacing the same `.alr`: the one higher in `mods_order` wins at the next start; swap them: the other
   wins. A player archive turned off moves to `disabled\`.
3. The same, applied from the main menu without restarting (model in the next match). Kill the game on the logos: the
   next start has no PDPL and says safe mode; the one after has mods again.
4. The menu with pad and keyboard; Settings and Theatre unchanged.
5. A test repo with one release; download, hash check, a corrupted hash refused.
6. A gsdata mod changes a skill's cost in a Test Arsenal battle; off: vanilla, no restart.
7. Two PCs: host with a mod, joiner without: browser shows it, the join downloads it, both see it, Unrated; leaving
   puts the joiner's own set back.
8. A catalog plugin that logs from DllMain: loads at the next start; a lobby needing it refuses with "needs a restart".
