# Conceptual branch notes

Very experimental work. This branch lives in its own folder, `D:\PhantomDust-Conceptual` (a git worktree of
`D:\PhantomDust-Dumped-NoMovie\_netmod`), so a master session and a Conceptual session can run at the same time.

## Things to know
- **One game install.** Whichever kit's Setup.bat was run last is the one the game runs, so testing a Conceptual kit
  and a master build means switching between them with their Setup.bat. To test a Conceptual kit: unzip it to its own
  folder and run its Setup.bat. To go back to the normal builds: run Setup.bat in the Experimental 72 folder again.
- **Merging master in happens here.** Git won't check out Conceptual in the main folder while this folder has it, so
  merges are done from this folder: `git merge master`, then `git push origin Conceptual`. Changes go from master into
  Conceptual only, never back.

## Rules
- Commit only to Conceptual; push only to `origin Conceptual`. Never force-push.
- Kits: `.\build.bat conceptual "what it tries"` saves `PhantomDust-pdnet Conceptual <N>.zip` on D:\ and, once the code is
  committed and pushed, publishes a pre-release `conceptual-<N>` with the asset `pdnet-conceptual-<N>.zip`. Installed
  Conceptual kits update from those only. Other kits ignore that asset name, and a Conceptual kit ignores update_channel.
  Off switch: Options > Updates (`conceptual_updates=off`). Never `build.bat experimental` or `promote` here.
- Notes for this work go in this file, not in HANDOFF.md; tools\kit-counter.txt stays untouched.
- `deps\eos-sdk` and `private\` are links to the main repo's; never commit private\ files, eos.ini, links.ini or game
  files.

## Feature assessment (2026-11-03)
Read-only: from the code, the game's data and Ghidra. No code written, nothing tried in the game. Sizes are relative
effort, not time.

### How matches sync
PCs don't exchange button presses. Each PC runs its own player and sends the others updates through 26 streams, table
at 0x1403fb4d0 (0x28 bytes each: +0 size, +4 sequenced/resent, +0x10 send, +0x18 receive, +0x20 name): PlayerPos,
CardKind, FireStartReq, FireReq, SkillActionCtrl, SkillActionEnd, Stage (2), CapsuleData, EnvCapture (2), CapsuleSteal,
CapsuleMove, PowSteal, Condition, DrawTimer, PlayerCaptureReq, PlayerCaptureAns, PlayerRevive, NoDmgTimer, DmgEffect,
Warp, ReadyStart, KilledCount, SkillHit (2). Per-frame send: FUN_14027abe0 (one packet per other slot, through
FUN_140125430).
- Each PC already gets the others' positions and hands, and pdnet carries all of it. CardKind's receive, FUN_14024dd90,
  writes four values (the four hand slots) into that player's status record.
- A replay is one PC's version of the match; PCs can disagree on bad connections ("cross lagging", HANDOFF section 11).
  Custom rules that decide a winner need the host to make the call.
- Waiting: FUN_14012bbb0 skips a frame when an active peer's last data is over 60 frames (0x3c) old, in network modes
  only (game mode 4 or 5). So PCs wait for stale peers, which is the freeze in HANDOFF. A spectator must not count as a peer.

### Each idea
| Idea | Possible? | Size | Main catch |
|---|---|---|---|
| Dynamic arsenal cases | Very likely | Small | Does the case's school count affect anything besides the editor limit? |
| Lobby settings | Very likely | Small to medium | All PCs must use the same values; in-lobby rule/type changes have crashed before |
| Dead camera | Very likely | Medium | Showing enemy hands to a dead player allows callouts over voice |
| Draft / Sealed | Likely | Sealed medium, Draft large | Must never alter the real save; the pick screen is the most work |
| New game modes | Depends on the mode | Small to large | Variants are easy; objective modes need host-decided scoring |
| Theatre | Likely | Large | Viewing a match with no local player is unproven |
| Spectator | Possible | Large | Full 2v2 matches have no free player slot |
| 3v3 or more | Possible but unlikely | Very large | Four players are built in throughout the game |

### How each would work
- **Dynamic cases:** replace the fixed limit with a count of the schools in the arsenal. Limit: case +0x50 (arsenal
  entries are 100 bytes, list at *(save + 0x10a*0x38)); the editor shows "You are limited to %d schools". Open: a maximum?
  What happens to buying and selling cases?
- **Lobby settings:** use the route of the host's nametag setting (PD_BODYTAGS, mpsd ApplyHostRules) and the spare lobby
  window (HANDOFF backlog 4). Custom matches are Unrated. Earlier in-lobby change attempt: HANDOFF backlog 3.
- **Dead camera:** a camera that follows any player, plus a freecam. The view is rebuilt, not their exact screen, and must
  snap back on revive. The local slot DAT_1404bbe00 is also used by the network send code, so flipping it isn't enough.
- **Game modes, easiest first:** rule variants (health, aura); kill-count or respawn modes (KilledCount, PlayerRevive
  exist); objective modes or uneven teams such as 3v1; team switching mid-match.
- **Draft / Sealed:** Quick (rule flags 0) hands out premade arsenals and Test Arsenal builds one, so the battle side is
  mostly there. Sealed first (a shared seed gives everyone the same packs), then Draft (live picks). Packs and rarities
  are a design job.
- **Theatre:** pdnet saves all match traffic and the match setup to a file, then plays it back. The setup needs saving
  because stage and rule travel in the host's 0x80-byte peer message. Forward only at first; rewind means restart and
  fast-forward.
- **Spectator:** Theatre fed live. Players' PCs send a copy of their traffic to the spectator, who needs no game slot and
  sends no acks. This also gives a tournament delay. Until then, a caster can stream a dead player's camera.
- **3v3:** the tables hold four entries and sit next to other data, so they can't grow in place. References: DAT_1404bc9d8
  about 500, DAT_1404bc608 about 180, DAT_1404bbe00 about 125. Masks (0xf), HUD, lobby and mesh loops also assume four.
  15 links instead of 6 makes cross lagging worse. Try uneven teams inside four players first.

### Suggested order
1. Three offline tests in Test Arsenal: follow the AI with the camera, record a match to a file, try a match with no
   local player. The third decides whether Theatre and Spectator are realistic.
2. Dynamic cases, lobby settings, dead camera.
3. Game mode variants, then Sealed, then Draft.
4. Theatre, then Spectator.
5. 3v3, only if everything else is done and a small test looks promising.

Anything that changes match behaviour needs every player on the same kit, and real tests need at least one other person.

## Mod loader and mod menu (plan, 2026-11-16, revised 2026-11-19)
Plan only: no code written, nothing tried in the game. The code plan, file by file: docs\MOD_LOADER_CODE.md.

**The idea (from the user):** a .txt list of GitHub repo links, one per mod; each creator uploads a zip to their
repo's releases. An in-game list with a checkbox per mod. Each mod has an ID; the host's mods go on the lobby, and
joining a lobby turns on and loads the host's mods. Plus an "auto sort" button for load order.

**Decision (the user, 2026-11-19): Torphedo's PDPL loads the mod files; Rebirth is the one place players go.** The kit
ships `pd_loader_core.dll` unchanged and pdlaunch injects it while the game is suspended. pdnet decides what's mounted
and in what order; fixes to PDPL go to Torphedo. (2026-11-16 had pdnet loading files itself. Torphedo pointed out that
PDPL's PhysicsFS mounts can change at any time, that its API hasn't changed in years, and that early loading is a
small change to how it's injected, not a reason to rewrite it. They were right: my "file list built once" was about
PDPL's start-up code only.)

### PDPL, from its code (commit 4264ea4; newest release 1.2.2)
- Its DllMain starts a thread that hooks `KERNELBASE!CreateFile2` (MinHook) and holds a mutex: the game's file opens
  wait until PDPL has mounted everything and loaded its plugins.
- PhysicsFS search path, first wins: loose files in `RoamingState\mods`, then every `.zip` / `.7z` in its root by
  name, then the game folder. Archive files are copied to `RoamingState\fake\<file name>` and that copy is opened.
- Plugin DLLs under VFS `/plugins/` (also inside archives) are loaded at start with MemoryModule.
- **1.2.2's DLL has no export table** (MSVC build, dumpbin, 2026-11-19), so pdnet can't call its PhysicsFS functions
  yet: changing mods without a restart waits on a PDPL build that exports them. (Mounting a path that's already mounted
  does nothing; unmounting fails while a file from it is open.)
- It calls `AllocConsole`, and its command loop reads stdin forever. pdnet handles both (step 1): a hidden console of
  its own first, so PDPL's fails, and pipes for PDPL's input (empty: no spinning) and output (to pdnet\pdpl.log).

### How Rebirth uses it
- **Next start:** pdnet keeps `RoamingState\mods` holding the enabled mods as hard links named `!rb00-<id>.zip`,
  `!rb01-...` in menu order, so PDPL mounts them early and in the right order (shaders, plugins). The player's own
  archives stay under their own names below ours; turning one off moves it to `mods\disabled\`.
- **Now (once PDPL exports PhysicsFS):** pdnet unmounts and remounts in the new order. Files read only at start
  (shaders, probably audio) need the restart; the menu says so.
- **Skill data:** PDPL redirects the gsdata file, but the game then copies its stored online copy over it, so pdnet
  still writes a mod's gsdata into the block at 0x1404c5240, working with the modpack guard.
- **Rebirth's own parts:** the Mods menu (Options > Mods: toggle, move, auto sort), the catalog and downloads with
  sha256, `pdmod.json`, PD_MODS on lobbies, the join check, Unrated for modded lobbies, safe mode (pdlaunch skips PDPL
  after a start that never reached the title screen).

### Risks
- **Code from lobbies:** plugin DLLs run with the game's rights. Lobbies only ever download catalog mods at the exact
  tag and sha256, and never load a plugin mid-session.
- **The remount race:** PDPL's hook checks a file and then asks where it is without a lock in between; a remount in
  that moment gives it nothing. Applies only happen outside matches; a lock from PDPL would close it.
- **Swapping mid-load:** no applies while a match loads or runs, and a match shouldn't start when members' PD_MODS
  differ.
- **A broken mod can stop the game starting:** safe mode.

### Requests for Torphedo (after their answers, 2026-11-19)
1. Export PhysicsFS (or a few PDPL functions) in release builds: needed for live changes.
2. `fake\` copies keyed by full path and deleted at start-up (agreed; exmot files need it).
3. `modlist.txt`: mount exactly what's listed, in order. Rebirth's Mods menu writes it; it replaces the `!rbNN-`
   links and `mods_enabled` / `mods_order`.
4. One `PHYSFS_getRealDir` call in the hook instead of `PHYSFS_exists` + `PHYSFS_getRealDir`.
5. A `CreateFileW` hook, if step 1 shows it's needed.
Torphedo's other answers: the console only shows on sideloaded copies (pdnet hides it anyway); `pdmod.json` is
Rebirth's metadata only, and `touches` is worked out from the paths at every scan, not written by creators.

### Steps
1. PDPL bundled and injected before the game resumes; a test archive changes a model, a map and a shader.
2. Scan, `mods_enabled` / `mods_order`, PDPL's mods folder kept in step (next start).
3. Live remount, safe mode.
4. The Mods menu.
5. Catalog and downloads.
6. Skill data from mods.
7. Lobbies.
8. Catalog plugins.

Status (2026-11-19): 1, 2 and 4-8 built (Conceptual 41-48); 3's live part waits on PDPL exports (safe mode is in). None tried in the game yet: the user tests everything at once with the list below.

### To test (all at once, newest kit)
Folders: PDPL's = `%LOCALAPPDATA%\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\RoamingState\mods`; Rebirth's =
`...\LocalState\pdnet\mods`. Copy mods into them (don't move). Logs: kit `pdlaunch.log`, `LocalState\pdnet.log`,
`LocalState\pdnet\pdpl.log` (Options > Send Log sends all three).

**PDPL in the game (step 1)**
1. No console window at start (Store copy, and an unpacked copy if there is one). pdnet.log: `pdpl: ready for PDPL
   (hidden console, ...)`.
2. pdlaunch.log: `mods: PDPL loaded (its file hook was in after N ms)` (not "wasn't seen in 5 s").
3. A zip in PDPL's folder with a changed model (`Assets/Data/player/pc01a/pc01a2.alr`) shows in a match.
4. The same with a map (`stNN.dat`) and a shader (`.cso` at the zip's root). Shaders are the open question.
5. The title screen still reads "Rebirth" (pdnet's title art and PDPL both hook file opens).
6. pdnet.log after 1 and 5 minutes: `pdpl: ... used N ms of CPU`: hundreds of ms is fine, tens of seconds is a busy loop.
7. `pdpl.log` lists the files the game opened as it opens them (`CreateFile2(): Opening ...`) and PDPL's own messages
   (`[Virtual Filesystem]: ...`); pdnet.log `pdpl: ready for PDPL (hidden console, read into pdnet\pdpl.log ...)`. Send it:
   it shows what loads at start vs per match.
8. Nothing slower: start-up time, menus, match loading.

**Mod folder sync (step 2)**
9. A zip in Rebirth's folder shows in the game; pdlaunch.log `mods: N mod(s), M on, in order: ...`, and PDPL's folder has
   `!rb000-<id>.zip`.
10. Two mods changing the same file: the higher one in the list wins.
11. A `.7z` mod: with 7-Zip installed its name shows in the list; without it, it still loads (no name or details).
12. Safe mode: close the game from Task Manager on the start-up logos; the next start has pdlaunch.log `starting without
    mods`, no mods in the game; the start after that has them again.

**Mods list (step 4)**
13. Multiplayer menu: the first item reads "Mods" with hint "Turn mods on and off, and choose which wins."; the Lobby
    Browser still works (search options, then the list). Opening Mods logs `Quick Join's search skipped` and no lobby is
    searched or joined.
14. Mods opens the list (title MODS), rows `[x] name  version  Online / Only you / PDPL`, hint with description, author,
    wins over / loses to.
15. The button bar: is the X icon right (`\a4#`, a guess)? Y should be right.
16. A toggles; LB / RB pages (needs 11+ mods); X picks up, up / down carries, X puts down; PDPL folder mods can't move.
17. Y auto sorts: a mod with `"load_after": ["other-id"]` ends up above it; a cosmetic (`"online": "local"`) mod at the top.
18. Back returns to the multiplayer menu; restart: the order and on / off stuck (settings.ini `mods_order`,
    `mods_enabled`), and the game uses them (a mod turned off is gone, its `!rb` file deleted).
19. Settings, Theatre and Custom Settings (same screen) still work as before.

**Catalog and downloads (step 5)** (add a mod with the Mod Admin app first; a release needs no sha256 line now, or set `mod_list_url=` in settings.ini to a
test catalog)
20. A catalog mod shows in the Mods list, off, right column "Download", hint "From the mod catalog ... Turn it on and it
    downloads at the next game start."
21. Turn it on, leave, restart: pdlaunch.log `newest release ...`, `downloading ...`, `downloaded, N bytes, sha256 ok`; it's in
    the game and in the list with its real name.
22. Game start isn't much slower with downloads (they happen while the game waits); a big mod on a slow line is the risk.
23. A catalog entry whose release has no .zip / .7z, or a repo that doesn't exist: the list's hint gives the reason; no
    crash, other mods fine.
24. No internet: the catalog from the last start is used; downloaded mods keep working.

**Skill data from mods (step 6)** (a mod with `Assets/Data/gstorage/gsdata_en.dat`, e.g. made with PD Helper's Skill Editor)
25. pdlaunch.log `mods: skill data (gsdata_en.dat, 278532 bytes) from <id>`; pdnet.log `skill data from mod <id>: N bytes
    written over the stored copy (the mod's changes to the game's file)`.
26. The mod's change (a skill's cost or damage) shows in the Arsenal editor and in a Test Arsenal battle.
27. It's still there after a match, after the lobby and after going back to the title screen (the guard keeps it).
28. Turn the mod off in the Mods list and restart: the skill is back to normal.
29. A PD Helper modpack (ModLoader) on top still works as before.
30. Skills the mod didn't change keep the Skill Update 1.40 values (compare one with a mod-less game).

**Mods on lobbies (step 7)** (two PCs; the host with a gameplay mod on, e.g. a map or skill mod, the joiner without)
31. Host's pdnet.log: `hosting with mods: <id>:<hash> (Unrated)`; the lobby's SESSION INFO reads `<rule> (Unrated, 1 mod)`.
32. Joiner's Lobby Browser: the lobby reads `(Unrated, 1 mod, restart to join)`; a vanilla lobby still reads Rated / Unrated.
33. Joiner picks it: the game says it couldn't join; pdnet.log `not joining lobby ...: its mods ... aren't this start's -
    restart the game and join again`; `LocalState\pdnet\lobby-mods.txt` exists.
34. Joiner restarts: pdlaunch.log `this start uses lobby ...'s mods`; a catalog mod is downloaded at the host's release; the
    Lobby Browser no longer says "restart to join"; joining works and both play with the mod.
35. A local (non-catalog) mod on the host that the joiner lacks: pdlaunch.log says it "isn't here"; the join is refused again.
36. The joiner's own "Only you" mods stay on during the lobby start; the start after that is back to their own list.
37. A modded match doesn't change ratings (pdnet.log `rating change ... not applied (gameplay mods: Unrated)`).
38. A modded player opening a vanilla lobby: `(Rated, no mods, restart to join)`; joining sets up a start without gameplay
    mods. `lobby_mods=off` in settings.ini: the join just fails, no lobby-mods.txt.

**Plugins (step 8)** (`powershell -File tools\test-mods\make-test-mods.ps1` makes `out\test-mods\test-plugin.zip` and
`test-plugin-core.zip`; copy both into Rebirth's mods folder)
39. Mods list: both show "Contains code (test_plugin.dll): it runs inside the game."; test-plugin-core also says it uses
    pd_loader_core.dll's functions and won't load.
40. pdnet.log / pdpl.log: `[test_plugin] loaded by PDPL in process ... - plugins work`. If it's missing, PDPL's loading of
    plugins (or the output pipe) doesn't work: send pdpl.log.
41. test-plugin-core: no "[test_plugin_core]" line, PDPL says it couldn't load it, and the game runs normally. (If it does
    load, PDPL exports more than dumpbin showed: good news, tell Torphedo's request list.)
42. A catalog mod with DLLs but no `plugin` flag in the catalog: the list says "has plugins (needs the catalog's plugin
    flag)" and it isn't loaded. With the flag (Mod Admin, Plugin on): it loads.

## Log
- 2026-11-21: GitHub's sha256 for mod downloads (Conceptual 50; the user asked, so modders only upload a zip). GitHub's API
  gives every release asset a "digest": "sha256:<hex>" worked out on upload. pdlaunch's release lookup (LookUpRelease) and
  the Mod Admin app's check now take a release's first .zip / .7z that has a sha256: a "sha256 <file> <hex>" line in the
  notes if there is one (it wins), else GitHub's digest. Tested: Torphedo's PDPL 1.2.2 release (no notes line) is now
  accepted by the admin app (sha256 from GitHub, 62a043e7...) and downloaded by pdlaunch -syncmods (275740 bytes, sha256
  ok); the test mod with a notes line still uses the line. The admin app lives outside the repo (nothing to commit there).
- 2026-11-19: Test session 1 fixes (Conceptual 49). The user's run on Conceptual 48 (this PC): PDPL's hook in after 15-16 ms,
  31 ms of CPU, title art fine, Mods list and toggles fine, mod-test downloaded (sha256 ok) and listed by name; but
  pdpl.log stayed empty and opening Mods still ran Quick Join's lobby search. (1) pdpl.log: a probe outside the game
  (scratchpad pdplprobe: loads pd_loader_core.dll as pdnet prepares the game, then 300 CreateFile2 opens) showed the
  pipe works, but PDPL's C runtime buffers a pipe in 4 KB blocks and the game never wrote that much in a minute. Now
  pdnet's hidden console is PDPL's standard output (a C runtime writes a console at once; VT processing on, so colours
  aren't kept as text) and pdnet reads its new rows every 100 ms (ReadConsoleText; buffer 400 x 9000, started again
  near the end); the pipe stays as the fallback without a console. Probe 2 confirmed every line comes through at once.
  (2) The multiplayer menu's Quick Join script starts the search as well as opening the session list (43), and "the game
  joins the first result": mods::ListOpened() when the pick becomes the Mods list, and MultiplayerService answers a
  Quick Join search within 3 s of it with no lobbies. Not yet tried in the game.
- 2026-11-19: Mod loader step 8, plugins (Conceptual 48). PDPL loads plugins/*.dll from the archives it mounts at start;
  catalog mods with DLLs already needed the catalog's plugin flag (step 5). New: pdlaunch lists each mod's plugin DLLs and
  reads their import tables (extracted with tar / 7-Zip, PeImports); a plugin importing pd_loader_core.dll gets a warning
  (PDPL 1.2.2's DLL exports nothing, so MemoryModule can't resolve it and the plugin won't load). The state file has
  the DLL names and the warning; the Mods list hint says "Contains code (...): it runs inside the game." plus the warning.
  tools\test-mods\make-test-mods.ps1 builds two test plugin mods: test_plugin.dll prints a line when loaded (PDPL's
  stdout is pdnet's pipe, so it lands in pdpl.log / pdnet.log), test_plugin_core.dll also imports
  plugin_get_module_handle from pd_loader_core.dll. Tested with pdlaunch -syncmods: both listed with their DLLs, only the
  core one warned. Not yet tried in the game.
- 2026-11-19: Mod loader step 7, mods on lobbies (Conceptual 47). PDPL can't change mods while the game runs, so a lobby with
  other mods is set up for the next start. Host: mods::LobbyTag (src\mods.cpp, from mods-state.txt: mods on, loaded, not
  "online": "local") goes in the lobby as PD_MODS (`id:<first 12 hex of the archive's sha256>[:<release tag>]`, comma-
  separated; pdlaunch now hashes each archive, cached in modinfo, and writes hash + tag to the state file); doc.rated = 0
  (Unrated) and ApplyRating skips modded matches. Joiner: before lobbies::Join, mpsd reads the lobby's PD_MODS; if it
  differs from this start's, the join fails (HandleNotFound) and mods::RequestForNextStart writes pdnet\lobby-mods.txt
  (lobby id, Unix time, the mods) unless lobby_mods=off. pdlaunch's next start (within 2 h, once) uses exactly those mods:
  catalog ones downloaded at the lobby's tag (releases/tags/<tag>, sha256 checked against the lobby's), others only if
  the same file, plus the player's own "online": "local" mods; the player then joins again. SESSION INFO: "(Unrated, N mods)",
  ", restart to join" when they differ. Tested with pdlaunch -syncmods: a fake lobby-mods.txt gave exactly the lobby's mods (a
  wrong-hash one reported), the next start was back to the player's own list. Not yet tried in the game (needs two PCs).
- 2026-11-19: Mod catalog admin (the user: the whitelist is controlled by a separate admin app, like the scoreboard, and
  mods are added by pasting a link). The Mod Admin app lives with the leaderboard admin tool, outside the repository:
  D:\PhantomDust-Dumped-NoMovie\admin-tool\Mod Admin.bat / ModAdmin.ps1 / modadmin.ini (never commit, push or ship it). It
  keeps the catalog as mods.txt on this repo's own mod-catalog branch (only that file; the first save makes it as a commit
  with no parent), written through GitHub's API with private\github-token.txt. Paste any GitHub link to a mod's repo
  (repo page, release, file, owner/repo), Check: it finds the repo, suggests an id, and checks the newest release has a
  .zip / .7z with a sha256 line (players' games refuse one without); Add saves at once (re-reading the file first). Also
  Remove, Plugin on / off (with a warning), Check all releases, Open repo, Copy repo link. pdlaunch's default
  mod_list_url is now that branch's mods.txt, so every kit reads one whitelist; mods\mods.txt in the repo only describes
  the format. Tested without the window and without writing to GitHub: link parsing, ids, release checks (this repo
  ok, Torphedo's refused for no sha256 line, missing repo), catalog round trip, reading (no branch yet). Not yet used to
  save: the first Add will create the branch.
- 2026-11-19: Mod loader step 6, skill data from mods (Conceptual 46): PDPL hands the game a mod's gsdata_<lang>.dat, but the
  game's stored online copy goes over the block (0x1404c5240) once, on the first main menu / join, so the mod is lost.
  pdlaunch now extracts the gsdata the game would get (loose files in PDPL's folder, else the highest mod that has it)
  to LocalState\pdnet\mod-skills\gsdata_<lang>.dat (+ .txt naming the mod) at every sync. SkillDataGuard, once the stored
  copy is in, writes the bytes where that file differs from the game folder's file (the whole file if those read the
  same), so untouched values keep the 1.40 numbers, and keeps the result as its modded state. Only while PDPL is in the
  game. Tested the extraction with pdlaunch -syncmods (full-size file with odd path capitals, priority, mod off = folder
  emptied); the in-memory part is not yet tried in the game.
- 2026-11-19: Mod loader step 5, catalog and downloads (Conceptual 45), all in pdlaunch at game start (launcher\modsync.cpp):
  the catalog mods\mods.txt (`<id> <owner/repo> [plugin]`; empty for now) from mod_list_url (default: this repo's Conceptual
  branch on raw.githubusercontent.com), cached in pdnet\modstore\catalog.txt. Catalog mods are off until turned on in the Mods
  list (shown with "Download"); for each one on, GitHub's releases/latest (cached 6 h with failures, modstore\releases.txt)
  gives the first .zip / .7z with a `sha256 <file> <hex>` line in its notes; downloaded to modstore\<id>\<tag>.zip, hash
  checked, newest 3 kept, then copied into PDPL's folder like the rest. A catalog mod with plugins/ DLLs needs the catalog's
  plugin flag. Tested with pdlaunch -syncmods and a test catalog: this repo's stable-15 release (pdnet-update-184.zip,
  11 MB) downloaded with sha256 ok; Torphedo's (no sha256 line) and a missing repo refused with reasons; bad line skipped;
  second run used the caches (1 s). Not yet tried in the game.
- 2026-11-19: Mod loader step 4, the Mods list (Conceptual 44): the user asked for Quick Join to become the mods list. Its
  pick opened the session list (43) straight from the multiplayer menu (36; this PC's log: '43 (from 36)' then 'Quick Join -
  any game type'; the Lobby Browser goes 36 -> 38 -> 43); HookedPushMenu turns it into menu 68 in a Mods mode (g_modsList)
  and the item reads Mods. Rows from pdnet\mods-state.txt (pdlaunch writes it; now with each mod's description): [x] name,
  version, Online / Only you / PDPL; hint: description, author, which mods it wins over or loses to, problems. A on / off, X
  pick up (the mod follows the cursor) / put down, Y auto sort (load_after / load_before, then cosmetic, models, maps,
  skills, plugins / other), LB / RB pages; the player's PDPL archives stay below in name order. Leaving saves mods_order /
  mods_enabled to settings.ini for the next start. The X icon (\a4#) is a guess. Not yet tried in the game.
- 2026-11-19: Mod loader .7z details (Conceptual 43): Windows' tar.exe can't read .7z ("LZMA codec is unsupported", LZMA2
  and PPMd alike), so pdlaunch reads .7z with 7-Zip when it's installed (registry Software\7-Zip Path64 / Path, or
  Program Files\7-Zip), and zips tar can't read too. Without 7-Zip a .7z still goes to PDPL, without name, files or
  conflicts. Tested with pdlaunch -syncmods: two .7z mods read, ids kept unique, conflicts counted. Not yet tried in the game.
- 2026-11-19: Mod loader step 2 (Conceptual 42): pdlaunch keeps PDPL's mods folder in step with the mod list at every
  start (launcher\modsync.cpp): mods from LocalState\pdnet\mods (Local), LocalState\pdnet\modstore\<id> (catalog, for
  later) and RoamingState\mods (the player's own); each archive listed and its pdmod.json read with tar.exe (cached in
  pdnet\modinfo); settings.ini mods_order / mods_enabled (new mods on); enabled Local mods copied in as !rb000-<id>.zip
  ...; the player's own archives turned off go to mods\disabled; LocalState\pdnet\mods-state.txt lists the result and
  conflicts. Safe mode: pdnet writes pdnet\mods-boot.txt when PDPL loads and deletes it past the title screen; pdlaunch
  skips PDPL if it's there. Tested on this PC with pdlaunch -syncmods on a test folder (order, off, on, renames,
  disabled\, conflicts): all as expected. Not yet tried in the game.
- 2026-11-19: Mod loader step 1 (Conceptual 41): PDPL 1.2.2 (deps\pdpl\pd_loader_core.dll, sha256 checked by build.ps1)
  goes in the kit; pdlaunch loads it after pdnet while the game is suspended and waits for its CreateFile2 hook (E9 at
  KERNELBASE!CreateFile2, up to 5 s; skipped with mods=off or when pdnet\mods-boot.txt exists). pdnet's PdBeforePdpl first
  makes a hidden console (PDPL's AllocConsole fails: no window) and pipes for PDPL's input (empty) and output
  (LocalState\pdnet\pdpl.log, also in Send Log); pdnet logs PDPL's CPU after 1 and 5 min. Mods go in
  %LOCALAPPDATA%\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\RoamingState\mods. Built; not yet tried in the game.
- 2026-11-19: mod loader plan and code plan changed to PDPL loading the files (the user's call, after Torphedo's reply). No code yet.
- 2026-11-16: mod loader code plan written (docs\MOD_LOADER_CODE.md). No code yet.
- 2026-11-16: Mod loader and mod menu: plan written (above); pdnet loads mods itself, not PDPL (the user's call). Read-only, no kit built.
(Newest first: what was tried, what happened.)
- 2026-11-19: Lobby Custom Settings, first version (not tried in the game). The user's choices: the lobby Options
  menu's Gamertag row becomes Custom Settings; health 5-99 (default 20), starting Aura 0-10 (0), Aura refill x0.5 / x1 /
  x1.5 / x2 (x1); anything off the defaults makes the match Unrated (until a Tournament scoreboard exists).
  - Menu: HookedAddItem renames "$GAMERTAG" in the lobby (menu 44). A on the lobby's Options window (window 4, 5+ rows)
    row 2 is kept from the script and opens the Settings screen (68) as Custom Settings (g_openingCustom ->
    g_customMatch in HookedPushMenu; the red hand on, as for the lobby's Arsenal editor). Three rows; left / right
    change them when this PC hosts (saved as match_health / match_aura / match_refill, and sent to the lobby at once),
    the others see the host's. Title CUSTOM SETTINGS, A does nothing, Back returns to the lobby.
  - Lobby: member attribute PD_MATCH "health,aura,refill" (MemberAttributes, and scores::PublishMatchSettings on a
    change); ApplyHostRules hands every PC the host's (SetHostMatchSettings). A host without it plays the defaults.
  - Match: HookCall at 0x14024e901 (FUN_14024e790's FUN_14024d020 call) swaps in the health and Aura for each player's
    main status record in online matches (game mode 4, 5); at battle start (labtests) DAT_1404bc958[0..3] *= refill.
    The rating hook gives 0 when the settings in use aren't the defaults.
- 2026-11-19: Lobby match settings, research (nothing built). Starting health and Aura: FUN_14024e790 sets up each
  player's status record with FUN_14024d020(record, object, 1, health, aura, ...), which writes health to +0xc, +0x10
  (max), +0x14 and Aura to +0x18 (int) and +0x1c (float). For a human player both come from gsdata's rule table:
  gsdata (DAT_1404c5228) + 0x33040 + rule (DAT_1404bc5dc) * 0x10 = { health, starting Aura, 99 or 5, 10 } (rule 11 uses
  +0x33100 / +0x33104; the AI takes health from DAT_1404c0188). gsdata_en.dat: rules 0-2 and 5-9 { 20, 0 }, 3 { 10, 0 },
  4 { 40, 5 }, 10 { 5, 0 }, 11 { 10, 0 }, 12 { 40, 0 }. The Aura refill rates (1.3 / 1.0 / 0.7 by school count) are at
  +0x33034, copied per player to DAT_1404bc958[slot] at setup. Route for the host's values: a member attribute like
  PD_BODYTAGS, read from the host member in xsapi\mpsd.cpp ApplyHostRules.
- 2026-11-16: Film nametag colours (Conceptual 40): in a film the ally colour goes to the watched player and their team
  in the film (snapshot teams, film::Info::team), the enemy colour to the rest; the flying camera keeps the last
  watched player's side. Before, the battle's own sides (this PC's slot 0) decided it.
- 2026-11-16: Remote hands on the HUD (Conceptual 39): watching a player other than this PC's, the hand showed plain
  knobs, no coloured cost badges. FUN_140151f90 draws a slot's badge only when its bit is set in the HUD's per-player
  mask (HUD object DAT_1404bec00 + player * 0x2c + 0x2e4; +0x2dc per slot is the draw-in state, 2-4 animate), and its
  number is the slot's cost (status record +0x360 = card struct +0x18; the card struct is record +0x348 + slot * 0x50:
  +8 in use, +0xc level, +0x28 its record, +0x30 skill). The card logic that sets them (FUN_140261a00, FUN_140262000,
  FUN_1402623b0: base +0x28 plus +0x2c per level from the skill table DAT_1404bdc50, 0x90 a skill, halved or doubled by
  condition 0x1e, at most 99) runs only on the owning PC. RemoteHandBadges sets them the same way before the HUD draw
  for a remote watched player.
- 2026-11-16: Film order and labels (Conceptual 38). The user saw 1v1s labelled Tag Team and no order in the list.
  - Labels: the snapshot's gameType (DAT_1405277b4) isn't the match type: the Nov 13 1v1s and the Nov 16 2v2s both
    record 1, the 3-player free-for-all 0. film::MatchType now reads the players and teams: 2 players One-on-One, more in
    2 teams Tag Team, else Battle Royale.
  - Order: this PC's clock jumped about an hour back and forth during the Nov 16 session (the log runs 18:20:05 then
    17:24:56 on one thread; a film named 18:20 was last written "17:24"), so films named and sorted by time came out of
    order. Films are now numbered as recorded: film-NNNNN-date-time.pdfilm, the last number in films\order.txt, never
    below the highest on disk. The list and the 100-film pruning sort by number (older unnumbered films after, by name).
  - Lobby: xsapi\mpsd.cpp tells the recorder the SearchableGame lobby id when it joins or creates it (film::SetLobby).
    Record 6 (Order) at a film's start holds its number, lobby session (+1 for each new lobby id) and match in that
    lobby, and the id. The Theatre row reads "Lobby 3  Match 2" (older films: their date); the hint adds the date and
    the lobby id's first 8 characters.
- 2026-11-14: Conceptual 36 test: after the film's battle ended, its controls stayed live (g_filmShown is cleared only
  when the next battle starts), so A on the results screen paused the game's update and it hung on a black screen.
  Conceptual 37: the film's controls, time control and text work only while the battle runs (match state 6), and the
  pause is dropped after it. Controls tweak (the user): no thrust; LT / Z held is slow motion, RT / C held fast forward.
- 2026-11-14: Conceptual 35 test: "the film no longer runs". Not the text change: a film picked after another battle in
  the same session played 0 records. The camera hook notices a battle's end only when the next one starts, after the
  film was queued, and EndOfBattle cleared the film's events. A queued film is now kept (Conceptual 36).
- 2026-11-14: Film text toggle (Conceptual 35): the film bar and the control reminders are one item, shown or hidden with
  D-pad Down / F2 (was X / F1), with a reminder line for it; the HUD (B / H) is separate. With the HUD hidden, the text
  sets the HUD's render states itself (FUN_140020c40(0, 3, 2) and (0, 4, 2), as FUN_140150c30 does before its text).
- 2026-11-14: Conceptual 33 test: the film plays (no faults), but some animations, shields in particular, don't show.
  FireStartReq and FireReq hold the frame the skill started on (int at +4, e.g. frame 95 sent at half-frame 48); the
  receivers queue them in the player's ring, which starts them at that frame. PlayerPos was already moved to the battle's
  clock (state+0x20), these weren't, so skills ran early or late against the movement. Conceptual 34 re-times them: as
  far behind this battle's frame as they were behind the sender's (capped at 120 frames). If shields still don't show,
  next suspects: whether they come through another stream, and the opening FireReqs with no FireStartReq (ids 100,
  120, 140).
- 2026-11-14: Conceptual 32 test: players stood still, two shared a spawn, the others were Guard. Causes:
  - The film reader had a sequenced stream's u32 number BEFORE its data; the game sends it AFTER (Condition's and
    DmgEffect's trailing u32 counts 0, 1, 2...; read that way, Condition names the sender's own slot and DmgEffect the
    skill 307 that player fired). Every stream 2-25 was fed 4 bytes off: that was Condition's fault (a "target slot" of
    5), likely DmgEffect's crash in Conceptual 29 too. Fixed in film::Load and tools/filmread.py; the FireStartReq skill
    id is the u16 at +20. DmgEffect is played again.
  - A fault stopped the whole film; now only that stream is left out from then on.
  - The third player was a copy of the opponent's entry, so its spawn too; the film moves it once it plays.
  - All 17 films so far are snapshot version 1, with no characters: their players can't be shown as what they played.
    Films recorded online with this branch's later kits (version 2) have them.
- 2026-11-14: Theatre film controls, from the user's Halo 3 film-controls card, each with a key (Conceptual 32):
  A / Space play-pause; RT (light pull slow motion x0.25, full pull fast forward x4) or Z / C held; Y / V flying cam or
  player cam; B / H HUD; X / F1 the controls overlay; D-pad / Q E switch players; left stick / WASD move; right stick /
  arrows look (orbit in player cam, R3 / T resets); RB LB / R F up and down; LT / Shift thrust; L3 / Ctrl held pan.
  Time control: the game's per-frame step FUN_1401c7630 (called at 0x1401c865a, from pdProcessManager's loop) runs every
  task group's update with FUN_1401cbd40, the world groups (1..0x1f) only when DAT_140530034 & 0x20001 is 0 (bit 0 = the
  game's own pause). Draws (FUN_1401cbcd0), and so the camera, run either way. HookedStep sets bit 0 for the step while
  paused, and on some frames for slow motion. For fast forward it runs the step several times a frame. The film is fed
  per step from a film clock that runs at the same rate (g_filmClock), not wall time. The overlay is drawn in the HUD
  hook with the nametag text call (FUN_1402477e0) at about x 24, y 300; it shows the time, PAUSED / SLOW MOTION / FAST
  FORWARD, the camera and the watched player. Not tested yet.
- 2026-11-14: Conceptual 30 test: the film decks loaded (both sides played with the film's skills), but a 3-player film
  showed only 2 players, and playback stopped 0.2 s in when Condition (stream 14) faulted. Its data names the target
  player (byte 2), here film slot 2, which didn't exist in the 2-player battle. Conceptual 31: every film player keeps
  their own slot. The battle setup (FUN_140128990) copies 4 entries of 0x48 (a player is there when its first int isn't 0),
  and the scoreboard battle fills 2. HookedTestStart fills entries 2 and 3 as copies of the opponent's entry (the AI's) with
  the film player's character, Aura rate 1.0 and deck counts built from the film. The film's names go into the name
  table (DAT_1404bca30) when playback starts. Not tested yet: the game mode stays One-on-One, and the extra players share
  the opponent's AI data (driven by the film anyway).
- 2026-11-14: Conceptual 29 crashed in film playback: DmgEffect (stream 20) faulted 24 s in (effect 0, type 0), then the
  game crashed at PDUWP+0x18c383 (FUN_14018c360, an effect instance with no resource data). Two more findings: both
  sides played with this player's own arsenal, so the film's skills were never loaded; and a sequenced stream's number
  isn't always new (DmgEffect kept #196915 while its data changed), so the "play only when the number moves on" filter
  dropped real messages. Fixes (Conceptual 30):
  - Film decks: film::Load gives each film player a deck: the recorded arsenal (version 2), or else the skills seen in
    their hand (CardKind, 4 shorts) plus the ones they fired (FireStartReq, u16 at +16; the hand alone missed most), repeated
    to 30. The scoreboard battle's two FUN_14014f830 calls (0x1402148da player, 0x14021491f opponent) are hooked to build
    each side's deck counts from that, so the battle loads them. Streams 2+ of a film player with no deck are skipped.
  - DmgEffect isn't played (the battle draws its own hit reactions). ReadyStart still skipped.
  - Copies: a film slot's stream is skipped only when it repeats the last played one exactly (number and data).
  - That film had 3 players (this PC was slot 2); only the first two are shown.
- 2026-11-14: Theatre playback plays skills. The first test (Conceptual 28) moved the players but showed no skill
  animations or sounds: only PlayerPos and CardKind were fed. Now every stream goes to its own receive function, read from
  the stream table (0x1403fb4d0 + i*0x28 + 0x18) and checked against the known addresses; all take (slot, state) with the
  data at state+0x40. Sequenced streams are resent until acknowledged, so each film slot's stream plays only when its
  number moves on. ReadyStart (22) is skipped: the battle has already started. The end log line counts skills started,
  fired, actions, hits and damage effects. Also: in film review the watched player gets a nametag too.
  Known limits: slot numbers inside the data (hit targets, steals) assume the film's slots match the battle's.
- 2026-11-14: Theatre playback, first version. A on a film loads it (film::Load decodes it, bounds-checked) and starts a
  Test Arsenal battle at once (no picks): the film's stage and layout, its characters when recorded, this player's current
  arsenal on both sides. At the battle's start every player is driven by received data and the film's PlayerPos and
  CardKind records go to the game's receive code at their recorded times; the 24 other streams (skills, hits, ...) are only
  counted for now. Watching uses the dead camera (follow anyone, orbit, free camera, their HUD). Film setup snapshot
  version 2: each player's character (DAT_1404bc420 + slot * 0x48 + 8), arsenal (deck counts DAT_140585fe8[slot] plus the
  hand) and Aura rate. Test Arsenal battles are no longer recorded. Not yet tried in the game.
- 2026-11-14: Conceptual 27 tried: A picks the highlighted film on either page (page 2 rows 0-6 the right films, empty
  rows 7-9 say no film there). The Theatre list is done apart from playback.
- 2026-11-14: Theatre tried on Conceptual 26: 17 films listed, paging both ways; A logged nothing (it never reaches session
  command 154 here). Now A is read in the button handler: the film on the highlighted row, the window's cursor at +0x12
  (window = manager +0x10402; FUN_14024b000 moves it). Seen there too: +0x16 rows, +0x18 rows shown and +0x1a the top row,
  so a window can scroll natively (a way to list all 100 films without pages). Not yet tried in the game.
- 2026-11-14: 100 films kept (was 25, the user). The Theatre lists all of them, 10 rows at a time: LB / RB (Page Up / Page
  Down, Q / E) turn the page, the hint says page N of M; the rows are buffers the window keeps reading, so a page only rewrites
  them. The list reads only each film's first 256 KB (setup) and last 28 bytes (end record: length), not whole files.
  Not yet tried in the game.
- 2026-11-14: Theatre menu, first part (the user: Theatre replaces Trade, a film list before the viewer). The multiplayer
  menu's Theater item (livetrade 40 from livemain 36) now opens the Settings screen (liveoption 68) as THEATRE: its rows are
  the newest 10 films (film::List: date, game type, length), each row's hint the stage and players; A logs the pick
  (playback not built). Title, button bar (Select / Watch / Back) and the Save question switch with it. Not yet tried.
- 2026-11-14: Theatre step 2, the film reader (toolsilmread.py, py -I toolsilmread.py <film> [--stream NAME]). Packet:
  7-byte mesh header (u16 sequence, u8 type 0x40 data / 0x01 heartbeat, u32 length), 8-byte message header (u16 length, 0x1200,
  u16 kind, u16 sender slot); kind 8 = the per-peer block (u64 mask: bit N stream N, bit 26+K ack of the K-th sequenced stream;
  u32 slot, u32 frame/2, u32 lag, u32 FUN_140184730), then the streams in order (sequenced ones with a u32 number first), a
  u32 per ack, the rest voice. Kinds 4, 10 and a kind-8 mask with its top bit set: end-of-match messages. All 15 online films
  decode with nothing left over; the crashed game's film was empty (bad magic, refused cleanly). The recorder now flushes each
  write, so a crash loses at most a second. Next (the user): the Theatre menu replaces Trade, with a film list before the viewer.
- 2026-11-14: Conceptual 24 tried (Test Arsenal, F9): followed the AI 37 s, free camera, off at the battle's end, no crash;
  the orbit controls turn the right way (the user).
- 2026-11-14: crash on Conceptual 23 (installed in the "Conceptual 22" folder) while following the AI (Test Arsenal, F9): PDUWP+0x15145e in the HUD draw, reading
  DAT_1404bcb50[object +0x13f20] +0xb8 when the HUD's player's camera block says a lock is on (+0x13f1c); that block is kept
  only for this PC's own player, and on the AI the entry was empty. Now, for the swapped HUD draw, a lock that doesn't lead to
  a real target is off. Dump: CrashDumps\PDUWP.exe.46292.dmp. Not yet tried in the game.
- 2026-11-14: follow camera controls (the user): the view stays on the watched player, as a player's own camera does, but the
  right stick (or Q E, R F) swings it round them and up / down; right stick click (or C) puts it back behind; a new player
  resets it. Not yet tried in the game.
- 2026-11-14: tried on Conceptual 22 (Test Arsenal, F9): facing read as way 0 after 5 s, camera shape 51.2 behind, 39.4 up,
  looking 5.5 ahead at 8.4 up; followed the AI, free camera, and off when the battle ended ("the battle is over").
- 2026-11-14: the dead camera now runs only while the battle does (match state 6); it stayed on over the results screens
  (states 12, 9, 10, 11) until the next match. Log: deadcam: off (the battle is over). Not yet tried in the game.
- 2026-11-14: the follow camera sat too close (the user). Its distance was the eye's offset along the facing only, which shrank
  whenever the camera looked at a target off to one side. Now the full horizontal distance, averaged, from frames where the
  camera is behind the player along its facing (log: follow camera shape). Not yet tried in the game.
- 2026-11-13: recorder tried online on Conceptual 20 (the user vs Motley, 2 players, 5 matches): a film per match, 1.6 to 5.7
  min, 142-184 KB a minute (budget 1 MB), nothing dropped, the files read back cleanly. Snapshot: game type 1, rule 0, stage 5,
  this PC slot 1, both names and teams, the arsenal. The 5.7-min film: 5266 packets out, 5256 in, mostly 87 bytes; the game
  frame counter moved 4377 in 340 s (about 13 a second online, 30 offline: to understand before playback).
- 2026-11-13: Theatre, step 1: the film recorder (srcilm.cpp; the user's REPLAY_SYSTEM_SPEC.md, strategy B: no server here,
  so a film is one PC's view). Each match: LocalState\pdnetilmsilm-<date>-<time>.pdfilm with every game datagram sent
  and received over EOS P2P (p2p.cpp), the time and the game frame, match states, and a setup snapshot at state 6 (type, rule,
  stage, layout, slots with name / team / health, this PC's arsenal). Written about once a second by a background thread,
  8 MB buffer cap (excess dropped and counted), newest 25 kept. Not the direct network mode. Other players' arsenals: not
  found yet (needed for playback). Not yet tried in the game.
- 2026-11-13: the log has an online run on Conceptual 16 (3-player Battle Royale, 01:11): the dead camera came on when the
  player went down, followed two players and the free camera, no errors. Skills not loading on the watched HUD, likely cause:
  only CardKind (+0x350, +0x378) comes over the network; the HUD shows the name of the player at +0x384 (card's source) in
  place of the skill when 0 or more, never set on another PC's player. Now -1 on such a player for the HUD draw. Unconfirmed.
- 2026-11-10: the follow view looked wrong (the user: own camera offset). Now from behind the watched player along their
  facing, object +0x12e68 (16-bit angle, in PlayerPos +0x18): how it maps to a direction is learnt from this player's own
  camera while it stands (8 readings, best match after 120 frames; log: facing angle read as way N), with the camera's shape
  measured the same way. Skills sometimes missing from the watched HUD: not understood yet; the hand fields are logged.
- 2026-11-10: following, the other players' tags were missing (the user). Now, whenever the view isn't the player's own,
  the game's tags are off for the HUD draw (its player's entries zeroed) and pdnet draws every player's tag from the camera
  actually used (a viewer that is nobody), except the watched player. Colours: dead, ally (this player, Tag partner), enemy.
  Not yet tried in the game.
- 2026-11-10: tags fixed (the user), but the down player's own tag was missing: the HUD never tags its own player and skips
  bodies unless body tags are on. Now drawn by pdnet after the HUD, the HUD's way (FUN_1402477e0 at the entry + (0x140, 0xe6),
  name DAT_1404bca30 + slot * 0x41, FUN_14028caa0), dead colour once down. The free camera's projection uses a viewer that is
  nobody, so it includes this player. As a body: only testable online. Not yet tried in the game.
- 2026-11-10: dead camera nametags (the user): in the free camera they floated, following a player there were none. The HUD
  places tags from the local player's camera block (+0x13f10, entries at +0x80, from FUN_140253c00 with that block's view).
  Now, while the view isn't the player's own, a stand-in block with the frame's real view (0x140584f40) is worked out the
  same way and lent to the HUD's player for the draw. Not yet tried in the game.
- 2026-11-10: PAUSED, lobby settings, nothing built yet. The user's choices: the host sets starting health, starting Aura
  level and an Aura refill multiplier (on top of the dynamic case rate) for everyone; no stage pick. Rating: custom matches
  rated only with a password set in the admin tool (outside this repo; HANDOFF 2026-10-28), on their own scoreboard named
  "Tournament", renamable from the admin tool if possible. To find out next: how the admin tool works and talks to EOS, a new
  EOS leaderboard / stats for Tournament (made in the Epic portal), and where the game sets starting health and Aura.
- 2026-11-10: dead camera tried offline on Conceptual 13 (Test Arsenal, F9): followed the AI, free camera, back to following,
  no errors; the HUD showed the AI's (the user). Free camera directions and the follow angle fine too. Online not tried.
- 2026-11-10: merged master (Public channel, skill previews, Lab Player 2 fix); Conceptual kits keep their own channel in
  master's update code (tag conceptual-<N>). Dead camera, first version (src\labtests.cpp): when this player is down (health
  record +0xc <= 0) in any match, the view follows a teammate (Tag Team) or anyone standing, with their HUD (the HUD draw at
  0x14047b8f0 runs with the local slot swapped), or a free camera. Arrows / D-pad pick and toggle; WASD QE RF or sticks and
  LB/RB move the free camera. F9 or View + D-pad Down pretends to be down in Test Arsenal. Not yet tried in the game.
- 2026-11-07: the slot label above an arsenal's name ("<prefix>NN") is gone, the user's request: FUN_14016d5f0 draws it only
  when its 6th argument is 0 or more; its three calls now pass -1. Story profile too. Not yet tried in the game.
- 2026-11-07: Max Credits & Skills gives an Arsenal Case each press, up to 16 (the user's choice). The main shop (menu 6)
  also offers one plain case: its stock commands (group 3 #310, #347) are wrapped. Multiplayer profile only. Not yet tried.
- 2026-11-07: the buy screen still said "1-School Arsenal Case": the script's texts are longer strings (the row holds the
  price too). The dialogs are now matched whole, and the row is changed as drawn (menu command 23) without its school
  number. Not yet tried in the game.
- 2026-11-07: confirmed in a Test Arsenal battle (the user; read from the running game's memory, read-only): the player's
  3-school arsenal refilled at 0.7x (DAT_1404bc958), the opponent's 2-school copy at 1.0x. The limit and its pop-up work.
  1.3x for a 1-school arsenal not yet seen.
- 2026-11-06: school limit pop-up. A refused 4th school gives no sign (the user), so it now uses the game's "3 of the same skill"
  pop-up (the -3 refusal) with its text swapped to the school limit, English only. Not yet tried in the game.
- 2026-11-06: school limit fix. The number followed the arsenal, but a 4th and 5th school went in: the save check's counters
  weren't current. Now a skill making a 4th school is refused as it's added (FUN_140159db0, its own -1 refusal), and the save
  check counts the arsenal itself. Not yet tried in the game.
- 2026-11-06: the case's number now also follows the arsenal live in the editor, as a visual indicator (the user's request).
  The limit of 3 moved to a wrap of the editor's check (0x140216cc0), which runs when saving ("too many schools"). The
  "School Limit" text is gone. An empty arsenal counts as 1. Not yet tried in the game.
- 2026-11-06: dynamic arsenal cases, the user's choices: the game's Aura rates, 3 schools at most, one plain case to buy.
  The school number (case +0x50) sets the editor limit and the Aura refill rate (1 school 1.3x, 2 1.0x, 3 0.7x). It now
  follows the arsenal in the multiplayer profile. Story unchanged.
  The buy texts are English only. Not yet tried in the game.
- 2026-11-06: offline tests passed on Conceptual 5 (the user: works; log and labtest.rec checked):
  - Update: Conceptual 4 installed build 5 by itself. As master notes, the keyboard needs the second start.
  - Camera: followed slot 1 for 27 s; its position, health and hand (it changed mid-battle) read fine.
  - Record: 447 frames in 14.9 s, 30 game frames a second, 200 bytes each, 5.9 KB a second (positions and hands only).
  - Play back: 447 of 447 frames with both players at +0x34 = 0, 0 faults; both ended at their recorded last position,
    and local control came back. So a match can run with nobody under local control, driven by recorded stream data.
  - Not covered: the other 24 streams (skills, hits), real network traffic, the HUD for another player, 2v2.
- 2026-11-06: Conceptual 5: F6-F8 never worked (1-4): the window gets no key events on this PC. Merged master's keyboard fix;
  the tests read pdlaunch's keys (KeysHeld), and also take View + D-pad (Left camera, Up record, Right play back).
  Not yet tried in the game.
- 2026-11-06: Conceptual kits get their own auto-update (the user's request; Conceptual branch only). Kits 1 and 2 can't
  update: install one built with it by hand once.
- 2026-11-06: Conceptual 2: in Conceptual 1 no test ran (log: labtest not installed); the camera call is a tail jump (e9) at
  0x14017d00e and the hook only accepted a call. Fixed. Not yet tried in the game.
- 2026-11-04: Conceptual 1, three offline tests in a Test Arsenal battle (src\labtests.cpp): F6 camera follow, F7 record,
  F8 play back with both players driven by the file (object +0x34 = 0). Built; not yet tried in the game.
- 2026-11-03: feature assessment written (above). Read-only, no kit built.
