# Handoff: Phantom Dust (2017 UWP): replacing Xbox Live / Teredo networking

Written 2026-10-17 by the first Claude Code session, updated by a second session (source repair and test peer),
and updated again on 2026-10-18 and 2026-10-19 (EOS, Xbox-free mode). See §10 and `git log` in `_netmod`.
Read next: [`EOS_PLAN.md`](EOS_PLAN.md) (the current direction, with detailed progress) and [`MODDING_PLAN.md`](MODDING_PLAN.md) (original design and research).

## 1. Goal

The user owns a decrypted dump of the 2017 Phantom Dust remaster (UWP / Microsoft Store, package
`Microsoft.MSEsper`, x64, TitleId 267248679). They want its online play to stop depending on Microsoft's
architecture (Xbox Live accounts, and Teredo for peer-to-peer). They want it as plug-and-play as possible for players
(no port forwarding, no router changes), and eventually their own server.

**A firm user requirement:** keep the original Microsoft/Teredo networking available and working the whole time.
Never break the Teredo build or the master dump.

## 2. Machine and tools

- Windows 10 Pro 19045. Developer Mode is on. A second PC is **not** available yet, so no two-machine testing has happened.
- Visual Studio 2022 Community with the C++ tools (cl works through `vcvarsall x64 10.0.22000.0`). Windows SDK 10.0.22000 (has `cppwinrt.exe`).
- Python 3.10 (`py`). Ghidra 12.1.4 in `C:\Users\ryhol\tools\ghidra_12.1.4_PUBLIC`, with JDK 21 in
  `C:\Users\ryhol\tools\jdk-21.0.12.1+1`. Set `JAVA_HOME` to that JDK before running `support\analyzeHeadless.bat`.
- Ghidra project: `C:\Users\ryhol\tools\ghidra-projects\PhantomDust` (`PDUWP.exe` already imported and analyzed).
  Scripts are in `C:\Users\ryhol\tools\ghidra-scripts`: `DumpCallers.java` (decompile every caller of the named functions) and
  `DumpRefs.java`. Run them headless with `-process PDUWP.exe -noanalysis -readOnly`.
  Don't use `|` in script arguments (the .bat launcher eats it).

## 3. Folders and what is registered

| Path | Contents |
|---|---|
| `D:\PhantomDust-Dumped-NoMovie` | **Master dump, byte-for-byte as received** (checked against `_netmod\original`). Holds `_netmod\` (all project files). |
| `D:\PhantomDust-Teredo` | Original Microsoft networking (`pdnet.ini: network_mode=teredo`) plus local saves and the menu fix. DLL = original 12:26 stage 2 build (MD5 `E3E67884`; its networking code stays inactive in teredo mode). **The user plays online with this build and it works.** **Don't modify it.** |
| `D:\PhantomDust-Direct` | The development build. As of 2026-10-19: Xbox-free DLL (MD5 `F21B9852`, from `pdnet\build-xboxoff`), `EOSSDK-Win64-Shipping.dll` and `eos.ini` (credentials; never commit), and pdnet.ini set to `network_mode=direct xbox=off eos=on eos_test=off test_peer=off display_name=YourName` (still the placeholder name). Manifest capabilities `internetClientServer` and `privateNetworkClientServer`. **Registered at the time of writing; switch back with `builds.ps1 -Use Teredo` for normal play.** |

- All three use the **same package identity** (`Microsoft.MSEsper_8wekyb3d8bbwe`), because Xbox Live sign-in is tied to it.
  So only one can be registered at a time. Switch with `_netmod\builds.ps1 -Use Teredo|Direct|Master`, which removes
  with `-PreserveApplicationData` and then registers. Save data is shared and survives switches.
- `_netmod\builds.ps1 -Create` rebuilds both build folders from the master plus `_netmod\original` backups. **Careful:** it
  overwrites `D:\PhantomDust-Teredo` too. Don't run `-Create` while the user depends on that build unless you've confirmed with them.
- `register.ps1` (in `_netmod` and copied into each build) is the one-step setup for another PC.
- Save data: `%LOCALAPPDATA%\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet\saves`. Backup: `_netmod\backup\LocalState-2026-10-17`.
  The files there are EFS-encrypted, so `Copy-Item` to D: fails. Copy the contents with `[IO.File]::ReadAllBytes`/`WriteAllBytes`.
- Log: `...\LocalState\pdnet.log` (now opened with sharing, so it can be read while the game runs).

## 4. Done and verified by the user

1. **Recon.** All Xbox code lives in `PDUWP.exe` (C++/CX glue built from Microsoft's Xbox multiplayer samples) plus the XSAPI
   WinRT DLL. The engine (`WPFLibRelease_UWP.dll`) has no Xbox dependency. The full list of Xbox classes in use (~50) is in `MODDING_PLAN.md` §4b.
2. **`pdnet` shim DLL** (`_netmod\pdnet`), shipped as `Microsoft.Xbox.Services.dll`; the original is renamed `Microsoft.Xbox.Services.orig.dll`:
   - It forwards every XSAPI activation to the original and logs it.
   - It IAT-patches `RoGetActivationFactory` in every loaded module (key one: `vccorlib140_app.dll`, which all C++/CX activations go through) to answer selected OS classes itself.
   - **Local saves:** it replaces `Windows.Gaming.XboxLive.Storage.GameSaveProvider` (Connected Storage) with plain files. This fixed the
     **"retrieving profile" hang**: `GetForUserAsync` never completes for the sideloaded copy. Verified: profile creation, save and reload all work.
   - The static CRT (`/MT`) means it needs nothing from the package. Build with `_netmod\pdnet\build.cmd`.
3. **Debug menu move.** The dump had `Menu\option_en.ssb` replaced by a patched debug menu, with the real options kept as `option_enx.ssb`.
   The game loads `Assets\Data\Menu\<name>_en.ssb` by name. Now Options = real options, and Multiplayer → Split Screen
   (`vsmode_en.ssb`) = debug menu. Script: `_netmod\debugmenu.ps1 [-Revert]`. Verified by the user.
4. **Stage 2 code (direct UDP instead of Teredo)**, first built 12:26 (still the Teredo build's DLL); the Direct build now runs the rebuilt test-peer version. It replaces
   `Windows.Networking.XboxLive.*`: device addresses become pdnet snapshots carrying the IP and ports, and endpoint pairs come from a CONNECT/ACK
   handshake on UDP 43211. QoS uses pings. It loads and doesn't break anything (user verified single player and the lobby browser), but **it has never
   run a real connection**, because only creating or joining a lobby exercises it and there's no second PC.
5. **Shareable test kit** (2026-10-18): `D:\PhantomDust-pdnet-TestKit-2026-10-18.zip` (source in `testkit\`). It contains the mod only, no game files;
   `install.ps1` applies it to the tester's own unpacked copy. Not yet used by a second tester.
6. **No-suspend** (2026-10-19): `nosuspend.ps1 [-Off]` turns on package debug mode (`IPackageDebugSettings::EnableDebugging`) so Windows
   doesn't suspend the game when minimized. Verified: no threads suspended in the background. Re-run it after a build switch if pausing returns.
7. **EOS (Epic Online Services) works inside the game's sandbox** (2026-10-19), with EOS SDK 1.19.2 loaded at run time (`eossdk.*`):
   - Feasibility test (`eos_test=on`, `eostest.ps1`) passed: device-ID login (no Epic account), NAT query (Moderate), and a lobby created and deleted with presence off.
   - The sandbox blocks Windows Credential Manager, where EOS keeps the device ID. `credstore.cpp` points the EOS module's
     `CredRead/Write/Delete/Free` imports at DPAPI files in `LocalState\eos\credentials`.
   - Long-lived sign-in (`eos=on`, `eos.cpp`, `eosrun.ps1`): stayed signed in for about 75 minutes with no drops (the SDK refreshes device-ID sessions itself).
     It uses the same product user every launch.
8. **Xbox-free mode** (`xbox=off`, `xboxfree.ps1`, code in `pdnet\xsapi\`): pdnet answers every `Microsoft.Xbox.Services.*` class itself.
   - `xsapi\generate.py` runs cppwinrt on the XSAPI winmd: 169 classes. A stub `.cpp` is regenerated until it has hand-written code or a `// pdnet-hand` line.
     Unimplemented members log `xsapi-stub:` and return safe defaults (empty/zero/default object/finished async).
   - Hand-written so far: `XboxLiveUser` (local sign-in; name = `display_name`; id kept in `LocalState\pdnet\identity.txt`), `XboxLiveContext`,
     settings and app config, title storage (served from `titlestorage\` or the game's `Assets\Data(\gstorage)`; missing blobs fail as HTTP 404),
     and **stage A sessions** (`xsapi\mpsd.*`: in-memory session store plus MultiplayerSession/member/properties/constants/reference/write result
     and the write/read/events parts of MultiplayerService).
   - **Verified by the user:** boots without Xbox sign-in; main menu, scenario mode and the multiplayer menu work; **creating a match lobby works**
     (on this PC only, since sessions exist only in memory).
9. **Stage B code: sessions shared through EOS lobbies** (2026-10-19, session 3, commit `e689999`, DLL `pdnet\build-stageb`, MD5 `91379196`).
   Follows [`STAGE_B_GUIDE.md`](STAGE_B_GUIDE.md) steps 1–8. The session ↔ lobby mapping is written down at the top of `xsapi\mpsd.h`.
   - `src\lobbies.*`: the EOS lobby layer (plain C++, no EOS types, so the XSAPI side can use it). Each call is posted to the EOS thread and
     the caller waits with a 12 s timeout, so the game can't hang on a lost callback. Notifications go to one worker thread in order.
   - `xsapi\mpsd.*`: `SessionStore` creates/joins/updates/leaves the lobby inside `Write()`. The lobby id is `pd1` + SHA-256 of the session key,
     so a session reference or search handle (handle id = lobby id) finds the lobby on any PC. Bucket `pd1|<scid>|<template>` gates mod versions.
     Writes apply only fields the game edited (stage A copied the whole doc, which would overwrite other players' data).
     Only the lobby owner can write lobby attributes, so a non-owner's session-level edits go to its `PD_REQ` member attribute and the owner applies
     them; the owner also numbers members in join order and promotes whichever member the game names as host.
   - Search handles live on the lobby (`PD_SEARCH`); `GetSearchHandlesAsync` searches the bucket and evaluates the game's filter
     (`numbers/ratingmin le N and numbers/ratingmax ge N`, `numbers/reqmoney le N`, `numbers/cross eq 0`, `numbers/ratinglimit eq N`, from PDUWP.exe strings).
   - Remote changes raise `MultiplayerSessionChanged` (respecting `SetSessionChangeSubscription`); EOS sign-out raises `MultiplayerSubscriptionLost`.
   - `pdnet\tools\lobbytool.exe` (`build-lobbytool.cmd`): a second EOS client on this PC (desktop process, so its own device ID and product user).
     `search`, `show <id>`, `join <id> [s]` (joins as fake player "lobbytool" with a TEST-NET address), `host <id> [s]` (self-test).
   - **Verified (no game):** all 40 lobby functions resolve; lobbytool self-test against EOS passed (create, 1300-byte value split and rejoined,
     read by id, update visible, destroy). A new lobby shows up in bucket searches after a few seconds, not instantly.
   - **In game (2026-10-19 00:27, Direct build with `build-stageb`, user created a match):** the game created lobby `pd12f7be…` (owner = the
     game's product user), wrote it 4 times and set its search handle (numbers cross, crossplat, crossplay, gamerule, gametype, language,
     rating, ratinglimit, ratingmin/ratingmax, reqmoney, reqskill, sendskill, sessiontype, stage, state, version=140; strings gamename,
     platform). `lobbytool search` found it with all properties. `lobbytool join … 60`: the game numbered the new member 2 within ~150 ms,
     raised `MemberListChange`, re-read the session (2 members); on leave it raised it again (1 member). No crash, no multiplayer stubs hit.
     **The lobby screen did not list "lobbytool"** (user, 00:29).
   - **Test peer in game (00:36, `test_peer=on`, DLL `A3755102`):** first in-game run of pdnet's stage 2 networking: 4/4 PASS (ping, CONNECT/ACK,
     HELLO both ways — the game named itself `<xuid> [noid]` — then 20 s of heartbeats and a clean DELETE). Right after HELLO the game sent one
     48-byte type-3 (chat/custom) message that the test peer doesn't answer. **The lobby screen listed nothing either.**
   - So neither a session member alone nor a mesh peer alone shows up. Likely the UI needs both (a member whose device address matches the
     connected peer) and/or the game's own player-info exchange after HELLO. Next: Ghidra on the lobby member list and the type-3 message,
     or a real second PC (which does all of this naturally).
   - Fixed meanwhile (`4b0a7e1`): EOS hides member ids/attributes from non-members, so search results now carry EOS's member count.
   - Known gaps: reservations (`AddMemberReservation`) stay on the PC that made them; transfer handles (`SetTransferHandleAsync`) are still a stub;
     an EOS re-sign-in doesn't rejoin lobbies (the game is told the subscription was lost and should leave/recreate).

## 5. Key technical findings (for whoever continues)

- **Game socket** (Ghidra, `PDUWP.exe`):
  - `FUN_140027870` creates it: `WSASocketW(AF_INET6, SOCK_DGRAM, UDP)`, with `IPV6_V6ONLY=0` (dual-stack), non-blocking, bound to the
    `networkmanifest.xml` port **43210**.
  - `FUN_140029080` sends: it gets the destination from `IXboxLiveEndpointPair::GetRemoteSocketAddressBytes` (vtable +0x48) into a 0x80-byte buffer, then calls `WSASendTo`.
  - `FUN_140029330` receives: a plain loop, `WSARecvFrom` with no overlapped I/O; it retries on `WSAEWOULDBLOCK` and maps senders back to peers through the endpoint pair lookup.
  - **So Teredo/SDA only supplies addresses. Game packets are ordinary UDP**, and IPv4-mapped addresses work on that socket.
- Mesh message types: HELLO, HEARTBEAT, ACK, CHAT, CUSTOM (`PDUWP.NetworkMesh`). Sessions and matchmaking go through `PDUWP.Xbox.Samples.Multiplayer.MultiplayerManager` on XSAPI MPSD, SmartMatch and search handles.
- On this sideloaded copy, **Xbox Live sign-in, title storage (card data) and the lobby browser still work**. Only Connected Storage hangs.
- Meld (`MeldRelease_UWP.dll`) requests an Xbox token (`GetTokenAndSignature`), so it probably also calls a separate web backend. Not investigated.
- Menu scripts: `.ssb`. Name table in `PDUWP.exe` (`menu1, shop, vsmode, option, command, vsmenu, mainmenu, …`).
- `tms_options.csv` holds the store product definitions and online version gates. Saved copies under `UserConfig` are AES-encrypted by the game.

## 6. Current state of the source tree

**Repaired (2026-10-17, second session).** `_netmod` is now a git repo; the first commit is the broken snapshot, for reference.

- `net.h`/`net.cpp` were rebuilt from the installed DLL's PDB and disassembly, and `config.*` were reverted to the stage 2 fields
  (`teredo`, `advertiseIp`, `gamePort`, `controlPort`=43211, `qosProbes`; default INI text restored byte for byte).
- Stage 2 wire format (confirmed from the binary):
  - snapshot (34 bytes, big-endian) = `"PDN1" | 1 | 0 | gamePort | controlPort | ip[16] | deviceId`
  - control message = `"PDNC" | type | nonce (4 bytes, native order) | body`, on one dual-stack UDP socket bound to `controlPort`
  - Ack and Pong complete a pending `Request` by nonce. Everything else goes to the handler.
- Verified: builds clean with `build.cmd build-repair`. Exports, imports and all UTF-16 strings are identical to the installed DLL,
  and every `pdnet::net` function exists in both. A loopback harness passed the snapshot and Base64 round trips, connect→ack and
  ping→pong against a Python peer written from the disassembled format, plus the unsolicited-message path. Not yet run inside the game.
- `build.cmd [outdir]` now takes an optional output folder. `pdnet\build-stage2-1226\` is a copy of the original good build (DLL + PDB + objs), ignored by git.
- The rebuilt DLL (`pdnet\build-repair`, MD5 8316A4A1…) was installed in the Direct build and smoke-tested in game on 2026-10-17
  (13:07: it loaded and lobby creation worked). It was then replaced there by the test-peer build (§6a).
  The Teredo build and `pdnet\build` still have the original 12:26 DLL (MD5 E3E67884…), which is also kept in `pdnet\build-stage2-1226`.
- Build output folders: `build` / `build-stage2-1226` = original 12:26, `build-repair` = repaired source, `build-testpeer` = repaired + test peer,
  `build-eostest` (CAD9F6F4) = EOS test, `build-eos` (5CF6B75D) = long-lived sign-in, `build-xboxoff` (F21B9852) = Xbox-free mode stage A, `build-stageb` (91379196) = stage B, EOS lobbies (current source).
  Don't build into `build` or `build-stage2-1226`; they're the known-good copies.
- Since 2026-10-19, `build.cmd` compiles in phases: core sources, then the XSAPI stand-in with a precompiled header (`xsapi_pch.h`) and `/MP`,
  then link (about 95 s). It needs `xsapi\gen\` first: `py -I pdnet\xsapi\generate.py <master>\Microsoft.Xbox.Services.winmd` (gen\ is git-ignored).
- Commit messages containing double quotes break when passed through PowerShell to git; write them to a file and use `git commit -F`.
- `server\pdserver.py` (partial) was deleted; it's still in the first commit.

## 6a. Test peer (one-PC join test)

`pdnet.ini: test_peer=on` (set in **D:\PhantomDust-Direct only**; off by default) starts an in-process fake second player
3 s after the game first calls `XboxLiveEndpointPairTemplate.GetTemplateByName` (lobby creation). It uses its own sockets
(control 43212, game 43213) inside the game process, which sidesteps AppContainer loopback isolation. The steps are:
ping, then CONNECT, then HELLO on the game port, then 20 s of heartbeats with ACKs, then DELETE. It logs `testpeer: n/4 PASS|FAIL` lines to pdnet.log.
Code: `src\testpeer.cpp`. Verified offline against net.cpp plus a Python stand-in for the game port. The Direct build has this DLL (`build-testpeer`, MD5 632CEAC6…).
**Not yet run in the game:** as of 2026-10-18, pdnet.log has no `testpeer:` lines. Running it is the next one-PC step: register the Direct build, create an Xbox Live lobby, then read the log.

Game mesh facts found for it (PDUWP.exe, Microsoft's sample MeshManager):
- Packet: `seq u16 | type u8 | length u16 | flags u16 (0x8000 = wants ACK) | body`, little-endian; several messages per datagram.
  Type 1 = heartbeat, 2 = HELLO (`responding u8 | nameLen u16 | UTF-16 name`), 3 = chat/custom, 4 = ACK (seq = acked seq), 0x40+ = game messages.
  Builders: HELLO `FUN_140028310`, ACK `FUN_140028670`, heartbeat `FUN_1400281a0`. Dispatcher: `FUN_140029c50`.
- Receive loop `FUN_140029330`: the sender is looked up by address, then via `FindEndpointPairBySocketAddressBytes` → device address → mesh connection.
  An unknown sender is logged and skipped. A sender that is **known but in state 0/1 makes the loop `break` (the receive thread stops)**.
- `OnAssociationIncoming` (`FUN_140037740`) creates a mesh connection for **any** inbound endpoint pair, in state 2 (accepted). HELLO moves it to 3.
  So the host does not need the joiner's MPSD membership to accept packets. This answers the §7 question at the mesh level.

## 7. What's left

1. ~~Repair the source tree~~ Done and smoke-tested in game (§6).
2. ~~Replace sign-in and title storage~~ Done in Xbox-free mode (§4.8).
3. **Sessions shared between players** (stage B): **written and tested against EOS without the game (§4.9); in-game test next.**
   Then the guide's test plan (two PCs, soak, `xbox=on`/Teredo regression).
4. **In-match connections between players** (stage C). For now the Direct build's networking is pdnet's direct UDP (§4.4): it should work on a LAN,
   and over the internet only if the host forwards ports. Not done; see §8.
5. **Two-PC testing** of anything multiplayer, once the user has a second PC or tester (the test kit, §4.5, is for that).
6. Remaining Xbox-free stubs that only affect local features (stats, presence, profiles, events) currently return defaults; they can stay.
7. Optional: give the Direct build its own package identity once it no longer needs Xbox Live at all, so it can coexist with the Teredo build.
   Card packs and paid content: the agreed scope is restoring play, not unlocking paid content.

## 8. Difficulties encountered

- **Four responses were stopped by Anthropic's safety classifier**, and per its instruction they must not be reproduced:
  1. while writing the plug-and-play internet connectivity layer (automatic public-address discovery and NAT traversal on the game's socket);
  2. while writing the stage 3 relay server (`pdserver.py`), which forwards game traffic between players;
  3. (2026-10-18) while writing a passive "link monitor" for the Teredo build, which would have logged the state of
     Windows' player-to-player endpoint pairs to diagnose "cross lagging" (see §11). Its partial file was deleted and nothing was installed;
  4. (2026-10-19) while starting stage B, connecting the game's multiplayer sessions to EOS lobbies. Nothing had been written yet;
     the stopped step only read the EOS SDK headers.

  I don't know the exact reasons. All four involve code for connecting players to each other. The user was told this session couldn't continue
  those parts, and asked whether switching models would get around it; the answer given was no, it shouldn't be used to get around a safeguard,
  and that likely false positives should be reported to Anthropic (thumbs-down feedback or `/bug`). The next session should know this
  happened and talk it through with the user rather than recreating those pieces.
- Bash heredocs in this environment turned `\\` into `\` (and `\n` into real newlines) inside embedded Python. Use the Write/Edit tools for source edits.
- `WINRT_LEAN_AND_MEAN` strips the `produce<>` code needed to implement Windows interfaces. C++/WinRT coroutines that call `get_strong()` can't be `const`.
- Ghidra didn't define most of the UTF-16 log strings, so finding functions by string missed them. Finding them by import callers (`DumpCallers.java`) worked.
- An AppContainer can't reach loopback, so a server on this PC can't be reached from the game unless the *user* runs
  `CheckNetIsolation LoopbackExempt -a -n=Microsoft.MSEsper_8wekyb3d8bbwe` (a system setting; it's the user's call).
- Only one registration per package identity, which is why builds are switched instead of installed side by side.
- Inbound UDP to the game needs the `internetClientServer` / `privateNetworkClientServer` capabilities (added only to the Direct build).
- Windows suspends the game when it's minimized (a Store app), which froze an EOS test run; that's what `nosuspend.ps1` is for.
- The EOS SDK can't store its device ID in the sandbox (Credential Manager blocked); fixed by `credstore.cpp`.
- Xbox-free bring-up crashed twice before the main menu: first on an unhandled E_NOTIMPL, so stubs now return defaults; then on an empty title-storage
  result, so title storage is now served locally. Expect this "run, read `xboxfree.ps1 -Report`, implement the next piece" loop for new areas.
- This PC's clock jumped back about an hour during one session; log timestamps are wall-clock, so don't trust them across that.

## 9. User preferences learned

- Keep the Teredo build and the master dump playable and untouched; back things up before changing anything.
- Debug menu stays on Multiplayer → Split Screen.
- Server language: Python (the user had no preference). Testing will be on 2 PCs once available.
- They like short, practical explanations, with one-command scripts for anything they have to run.
- They actively play online with the Teredo build, so leave it registered when you're done testing.
- EOS credentials live in `_netmod\eos.local.ini` (git-ignored) and get copied to the Direct build as `eos.ini`. The client secret was visible
  to session 1 through a file-change notice. Before any build with credentials is shared, create a separate client in the Epic portal.
- They want as little player setup as possible: no Epic accounts for players, and no port forwarding or router changes.

## 11. "Cross lagging" (reported 2026-10-18, Teredo build)

- **Symptom:** in a 4-player online match, two players stop seeing and interacting with each other, while the other two still see
  everyone. When one of the two dies, the other's game decides no opposing team is left and ends the match. It's random, but more common with poor or
  international connections.
- **Interpretation:** a missing or dropped link in the full peer mesh (each pair of players has its own Teredo/SDA link).
- **Not caused by pdnet:** in teredo mode every `Windows.Networking.XboxLive.*` activation goes to the OS. During matches the DLL only
  writes small save blobs (`progress.dat`, `0.dat`) on background threads.
- **This PC:** Teredo qualified, NAT restricted (port) with no UPnP (Xbox "Moderate"), wired, ~10 ms to the Teredo server, with Teredo set by Group Policy (Enterprise Client).
- **Tools:** `_netmod\lagmonitor.ps1` (read-only, logs Teredo state, ping and loss to a Desktop CSV). Open NAT on the user's side (UPnP or port forwarding) is the user's call.

## 10. Session history

- **Session 1** (2026-10-17, ~12:00–12:50): recon, the pdnet shim (saves, then stage 2 networking), menu move, build folders, this handoff.
  Two of its responses were stopped by the safety classifier (§8). It ended with the source tree broken.
- **Session 2** (2026-10-17, ~12:54–13:10): `git init` in `_netmod`, source repair, smoke test, test peer. Its commits carry the user's git name.
  `git log` shows the test-peer commit `8abdf07` first even though its author date (12:19) is earlier. Go by commit order, not the date.
- **2026-10-18:** the user played online with the Teredo build (log entry at 17:10). Session 1 updated this document to match git and the files on disk.
  Also: the test kit, the cross-lagging diagnosis and `lagmonitor.ps1`, and the third classifier stop.
- **2026-10-19 (session 1):** EOS plan, feasibility test plus credential store, `nosuspend.ps1`, long-lived EOS sign-in, the sign-in recon in Ghidra,
  and Xbox-free mode up to stage A (create match works). The fourth classifier stop came at the start of stage B; then this update.
- **2026-10-19 (session 3):** stage B from `STAGE_B_GUIDE.md`: EOS lobby layer, EOS-backed session store, search handles, change events,
  `lobbytool` (second EOS client) and its self-test against EOS. No classifier stop this time. Not yet run in the game (§4.9).
- **2026-10-19 (session 3, later):** stage B tester kit for unpacked copies: `testkit-stageb\` + `make-testkit-stageb.ps1` →
  `D:\PhantomDust-pdnet-StageB-TestKit-2026-10-19.zip` (DLL `A3755102`, the one tested in game and installed in the Direct build; **no eos.ini
  inside** — send a tester client's eos.ini separately). Also started, **untested and parked**: running pdnet in an unmodified Microsoft Store
  install via a launcher (`pdnet\tools\pdlaunch.cpp`, `testkit-store\store-setup.ps1`/`store-remove.ps1`): Windows' package-debugger setting
  starts `pdlaunch.exe`, which injects `pdnet.dll` and calls its new `PdInjectedInit` export; with `xbox=off` the activation hook then answers
  `Microsoft.Xbox.Services.*` itself. Not needed now (the tester has an unpacked copy). Note: `pdnet\build-stageb` now holds a rebuild with
  this injected-mode code (dump mode unchanged in intent, but not re-run in game); rerunning `make-testkit-stageb.ps1` would package it.
