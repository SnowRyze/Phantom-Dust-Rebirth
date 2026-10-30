# Handoff: Phantom Dust (2017 UWP): replacing Xbox Live / Teredo networking

Written 2026-10-17 by the first Claude Code session, updated by a second session (source repair and test peer),
and updated again on 2026-10-18 and 2026-10-19 (EOS, Xbox-free mode). See Â§10 and `git log` in `_netmod`.
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
- GitHub (from 2026-10-27): `_netmod` is pushed to https://github.com/SnowRyze/Phantom-Dust-Rebirth (remote `origin`; branches master and kit27, tags stable-N). Every code change gets a simple commit, a note in this file, and a push.

## 4. Done and verified by the user

1. **Recon.** All Xbox code lives in `PDUWP.exe` (C++/CX glue built from Microsoft's Xbox multiplayer samples) plus the XSAPI
   WinRT DLL. The engine (`WPFLibRelease_UWP.dll`) has no Xbox dependency. The full list of Xbox classes in use (~50) is in `MODDING_PLAN.md` Â§4b.
2. **`pdnet` shim DLL** (`_netmod\pdnet`), shipped as `Microsoft.Xbox.Services.dll`; the original is renamed `Microsoft.Xbox.Services.orig.dll`:
   - It forwards every XSAPI activation to the original and logs it.
   - It IAT-patches `RoGetActivationFactory` in every loaded module (key one: `vccorlib140_app.dll`, which all C++/CX activations go through) to answer selected OS classes itself.
   - **Local saves:** it replaces `Windows.Gaming.XboxLive.Storage.GameSaveProvider` (Connected Storage) with plain files. This fixed the
     **"retrieving profile" hang**: `GetForUserAsync` never completes for the sideloaded copy. Verified: profile creation, save and reload all work.
   - The static CRT (`/MT`) means it needs nothing from the package. Build with `_netmod\pdnet\build.cmd`.
3. **Debug menu move.** The dump had `Menu\option_en.ssb` replaced by a patched debug menu, with the real options kept as `option_enx.ssb`.
   The game loads `Assets\Data\Menu\<name>_en.ssb` by name. Now Options = real options, and Multiplayer â†’ Split Screen
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
   Follows [`STAGE_B_GUIDE.md`](STAGE_B_GUIDE.md) steps 1â€“8. The session â†” lobby mapping is written down at the top of `xsapi\mpsd.h`.
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
   - **In game (2026-10-19 00:27, Direct build with `build-stageb`, user created a match):** the game created lobby `pd12f7beâ€¦` (owner = the
     game's product user), wrote it 4 times and set its search handle (numbers cross, crossplat, crossplay, gamerule, gametype, language,
     rating, ratinglimit, ratingmin/ratingmax, reqmoney, reqskill, sendskill, sessiontype, stage, state, version=140; strings gamename,
     platform). `lobbytool search` found it with all properties. `lobbytool join â€¦ 60`: the game numbered the new member 2 within ~150 ms,
     raised `MemberListChange`, re-read the session (2 members); on leave it raised it again (1 member). No crash, no multiplayer stubs hit.
     **The lobby screen did not list "lobbytool"** (user, 00:29).
   - **Test peer in game (00:36, `test_peer=on`, DLL `A3755102`):** first in-game run of pdnet's stage 2 networking: 4/4 PASS (ping, CONNECT/ACK,
     HELLO both ways â€” the game named itself `<xuid> [noid]` â€” then 20 s of heartbeats and a clean DELETE). Right after HELLO the game sent one
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
- Menu scripts: `.ssb`. Name table in `PDUWP.exe` (`menu1, shop, vsmode, option, command, vsmenu, mainmenu, â€¦`).
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
  and every `pdnet::net` function exists in both. A loopback harness passed the snapshot and Base64 round trips, connectâ†’ack and
  pingâ†’pong against a Python peer written from the disassembled format, plus the unsolicited-message path. Not yet run inside the game.
- `build.cmd [outdir]` now takes an optional output folder. `pdnet\build-stage2-1226\` is a copy of the original good build (DLL + PDB + objs), ignored by git.
- The rebuilt DLL (`pdnet\build-repair`, MD5 8316A4A1â€¦) was installed in the Direct build and smoke-tested in game on 2026-10-17
  (13:07: it loaded and lobby creation worked). It was then replaced there by the test-peer build (Â§6a).
  The Teredo build and `pdnet\build` still have the original 12:26 DLL (MD5 E3E67884â€¦), which is also kept in `pdnet\build-stage2-1226`.
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
Code: `src\testpeer.cpp`. Verified offline against net.cpp plus a Python stand-in for the game port. The Direct build has this DLL (`build-testpeer`, MD5 632CEAC6â€¦).
**Not yet run in the game:** as of 2026-10-18, pdnet.log has no `testpeer:` lines. Running it is the next one-PC step: register the Direct build, create an Xbox Live lobby, then read the log.

Game mesh facts found for it (PDUWP.exe, Microsoft's sample MeshManager):
- Packet: `seq u16 | type u8 | length u16 | flags u16 (0x8000 = wants ACK) | body`, little-endian; several messages per datagram.
  Type 1 = heartbeat, 2 = HELLO (`responding u8 | nameLen u16 | UTF-16 name`), 3 = chat/custom, 4 = ACK (seq = acked seq), 0x40+ = game messages.
  Builders: HELLO `FUN_140028310`, ACK `FUN_140028670`, heartbeat `FUN_1400281a0`. Dispatcher: `FUN_140029c50`.
- Receive loop `FUN_140029330`: the sender is looked up by address, then via `FindEndpointPairBySocketAddressBytes` â†’ device address â†’ mesh connection.
  An unknown sender is logged and skipped. A sender that is **known but in state 0/1 makes the loop `break` (the receive thread stops)**.
- `OnAssociationIncoming` (`FUN_140037740`) creates a mesh connection for **any** inbound endpoint pair, in state 2 (accepted). HELLO moves it to 3.
  So the host does not need the joiner's MPSD membership to accept packets. This answers the Â§7 question at the mesh level.

## 7. What's left

1. ~~Repair the source tree~~ Done and smoke-tested in game (Â§6).
2. ~~Replace sign-in and title storage~~ Done in Xbox-free mode (Â§4.8).
3. **Sessions shared between players** (stage B): **written and tested against EOS without the game (Â§4.9); in-game test next.**
   Then the guide's test plan (two PCs, soak, `xbox=on`/Teredo regression).
4. **In-match connections between players** (stage C). For now the Direct build's networking is pdnet's direct UDP (Â§4.4): it should work on a LAN,
   and over the internet only if the host forwards ports. Not done; see Â§8.
5. **Two-PC testing** of anything multiplayer, once the user has a second PC or tester (the test kit, Â§4.5, is for that).
6. Remaining Xbox-free stubs that only affect local features (stats, presence, profiles, events) currently return defaults; they can stay.
7. Optional: give the Direct build its own package identity once it no longer needs Xbox Live at all, so it can coexist with the Teredo build.
   Card packs and paid content: the agreed scope is restoring play, not unlocking paid content.

## 8. Difficulties encountered

- **Four responses were stopped by Anthropic's safety classifier**, and per its instruction they must not be reproduced:
  1. while writing the plug-and-play internet connectivity layer (automatic public-address discovery and NAT traversal on the game's socket);
  2. while writing the stage 3 relay server (`pdserver.py`), which forwards game traffic between players;
  3. (2026-10-18) while writing a passive "link monitor" for the Teredo build, which would have logged the state of
     Windows' player-to-player endpoint pairs to diagnose "cross lagging" (see Â§11). Its partial file was deleted and nothing was installed;
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
- Debug menu stays on Multiplayer â†’ Split Screen.
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

- **Session 1** (2026-10-17, ~12:00â€“12:50): recon, the pdnet shim (saves, then stage 2 networking), menu move, build folders, this handoff.
  Two of its responses were stopped by the safety classifier (Â§8). It ended with the source tree broken.
- **Session 2** (2026-10-17, ~12:54â€“13:10): `git init` in `_netmod`, source repair, smoke test, test peer. Its commits carry the user's git name.
  `git log` shows the test-peer commit `8abdf07` first even though its author date (12:19) is earlier. Go by commit order, not the date.
- **2026-10-18:** the user played online with the Teredo build (log entry at 17:10). Session 1 updated this document to match git and the files on disk.
  Also: the test kit, the cross-lagging diagnosis and `lagmonitor.ps1`, and the third classifier stop.
- **2026-10-19 (session 1):** EOS plan, feasibility test plus credential store, `nosuspend.ps1`, long-lived EOS sign-in, the sign-in recon in Ghidra,
  and Xbox-free mode up to stage A (create match works). The fourth classifier stop came at the start of stage B; then this update.
- **2026-10-19 (session 3):** stage B from `STAGE_B_GUIDE.md`: EOS lobby layer, EOS-backed session store, search handles, change events,
  `lobbytool` (second EOS client) and its self-test against EOS. No classifier stop this time. Not yet run in the game (Â§4.9).
- **2026-10-19 (session 3, later):** stage B tester kit for unpacked copies: `testkit-stageb\` + `make-testkit.ps1 -Stage B` â†’
  `D:\PhantomDust-pdnet-StageB-TestKit-2026-10-19.zip` (DLL `A3755102`, the one tested in game and installed in the Direct build; **no eos.ini
  inside** â€” send a tester client's eos.ini separately). Also started, **untested and parked**: running pdnet in an unmodified Microsoft Store
  install via a launcher (`pdnet\tools\pdlaunch.cpp`, `testkit-store\store-setup.ps1`/`store-remove.ps1`): Windows' package-debugger setting
  starts `pdlaunch.exe`, which injects `pdnet.dll` and calls its new `PdInjectedInit` export; with `xbox=off` the activation hook then answers
  `Microsoft.Xbox.Services.*` itself. Not needed now (the tester has an unpacked copy). Note: `pdnet\build-stageb` now holds a rebuild with
  this injected-mode code (dump mode unchanged in intent, but not re-run in game); rerunning `make-testkit.ps1 -Stage B` would package it.
- **2026-10-19 (session 3): FIRST TWO-PC TEST PASSED (stage B + direct UDP), user + one tester on different home networks.** Kit
  `PhantomDust-pdnet-StageB-TestKit-WITH-EOS.zip` (DLL `A3755102`, tester EOS client). Run 1: user joined the tester's lobby through the EOS
  browser and lobby join worked, but the match connection timed out (host's advertised address was a private LAN address on another network).
  Run 2: user hosted with UDP 43210/43211 forwarded and `advertise_ip=<public IP>`; the tester found and joined the lobby, connected directly
  (`sda: inbound connection ... (seen as <tester's public IP>)`), and **the user reports everything running correctly** (lobby and match).
  Remaining: automatic internet connectivity without port forwarding (stage C, e.g. EOS P2P relays â€” see Â§8 before starting), longer soak,
  `xbox=on`/Teredo regression check.
- **2026-10-20 (session 3): stage C â€” match traffic through EOS P2P (`network_mode=eos`), commit `76c4d44`, DLL `pdnet\build-stagec`
  (MD5 `C5F9697B`), design at the top of `src\p2p.h`.** Device addresses v2 carry the EOS product user id; pairs to v2 peers handshake over
  EOS P2P and the game sends to a local "link" socket that pdnet bridges to the peer (Epic does NAT traversal, relays as fallback). v1 peers
  (older kits, other modes) keep direct UDP â€” so both players need this build in `network_mode=eos`. EOS thread pumps every ~2 ms
  (`timeBeginPeriod(1)`). Every 10 s each link logs ping/jitter/loss/path/traffic (`p2p: link ...`).
  **One-PC test passed in game (Direct build, 00:09):** `lobbytool p2ppeer 20` = 4/4 PASS over EOS P2P (ping, connect/ack, HELLO both ways,
  heartbeats, delete), ping 3â€“4 ms, 0 loss. `lobbytool p2ppeer 30 force` (Epic relays only) = 4/4 PASS, ping ~42 ms, 0 loss.
  Known issue: the direct/relayed label is unreliable (EOS reported "direct" with forced relays; the game side kept a stale label because no
  close event came between runs) â€” use the ping. Next: two-PC test with no port forwarding, then measure real matches; redundancy later.
- **2026-10-20: stage C tester kit** `testkit-stagec\` (README for no port forwarding, `network_mode=eos`) built with
  `make-testkit.ps1 -Stage C -EosIni .\eos.tester.ini` â†’ `D:\PhantomDust-pdnet-StageC-TestKit-C5F9697B-with-eos.zip`. Kit zips are now named
  after the DLL's MD5 prefix (the clock jumps made dated names ambiguous). Both players need stage C; the user's Direct build already matches.
- **2026-10-20: wrong card names in Xbox-free mode** ("Aura Particle" shown as "Rebirth", still behaving as Aura). Cause: the dump's
  `Assets\Data\gstorage\gsdata_en.dat` (C519B2D8, 2022-07-12) is an edited copy; next to it are `gsdata_en.dat.bak` (984175A8) and
  `gsdata_en - Copy.dat` (892EEDB6, 2022-07-06, header matches the untouched gsdata_ja.dat). Card text is identical in all three; the header
  differs: u32 at 0x14 = 374 originally (750 edited) and u32 at 0x18 = 301 originally (300 edited, off by one = names shifted by one entry).
  On Xbox Live the game downloaded the real blob, so this only shows with pdnet serving local files. Fix (Direct build only): the original
  copy as an override, `D:\PhantomDust-Direct\titlestorage\gsdata_en.dat` (pdnet checks `titlestorage\` first). Master and Teredo untouched.
  Not yet confirmed in game. Testers with the same dump would see the same bug.
- **2026-10-19/20: crash when leaving a lobby (both players)** â€” access violation at PDUWP.exe+0xA7083. Ghidra: `FUN_14009c7c0` is the
  game's session-write task; it polls `TryWriteSessionAsync` every frame and calls `GetResults` on it more than once, then reads
  `WriteSessionResult.Succeeded` without a null check. C++/WinRT coroutines move their result out on the first `GetResults`
  (`return std::move(m_result)` in windows.foundation.h), so the second read was null. Fix: `xsapi\repeatable.h` wraps every implemented
  MultiplayerService operation (writes, reads, by-handle, search) so results can be read again. Build `build-stagec` MD5 `61C45C57`.
  **Not yet confirmed in game.** Other stand-ins still use plain coroutines; wrap any that crash the same way.
  Installed in the Direct build (61C45C57) and packaged as `D:\PhantomDust-pdnet-StageC-TestKit-61C45C57-with-eos.zip` (replaces the
  C5F9697B kit, which has the leave crash).
  **Confirmed by the user (2026-10-19): leaving a lobby no longer crashes; stage C kit 61C45C57 works between the two PCs.**
- **2026-10-20: nametag investigation (user wants nametags kept on dead bodies).** The over-head nametag is the white gamertag (from the
  session, i.e. pdnet). Ghidra: players in a 4-entry table `DAT_1404bc9d8` (position at +0x12e40, flags +0x10/+0x20/+0x24), names for HUD
  panels in `DAT_1404bca30` (65 bytes each); HUD panels/scoreboard (`FUN_140155310`, `FUN_140150c30`, `FUN_1401ca1e0`) draw via
  `FUN_1402477e0` â€” none of that is the floating nametag, which is probably remaster code reading the gamertag via the XSAPI classes.
  New debug option `name_trace=on` (`xsapi\nametrace.*`, commit `9acf2fe`, DLL `pdnet\build-nametrace` 7D824149, not shipped): hardware
  breakpoints on copies of other players' gamertags, logs the PDUWP.exe call chains that read them. One-PC check (lobbytool in the lobby):
  works, no crash, 23 heap copies, reads from chains under `140009b06 <- 140008953 <- 1400c58f6`. Next: a real match with the tester while
  name_trace is on (only the PC that should see the tags needs it), someone dies, then compare chains during life vs after death.
  The Direct build currently runs build-nametrace with name_trace=on; switch back to build-stagec (61C45C57) for normal play.

## Discord lobby watcher (lobbytool watch)
`pdnet\tools\out\lobbytool.exe watch` searches the Phantom Dust lobby bucket every `interval_seconds` (default 15, min 10) and posts to a Discord webhook:
- new lobby -> "opened a lobby" (lobbies already open when the watcher starts are only logged)
- member count change -> "n/max players"
- gone from search twice -> Read by id: still there with members -> "full" (search hides full lobbies), else "closed"
- lobbies with 0 members are ignored (Epic keeps an empty lobby for a while after the host leaves)
Settings: `lobbywatch.ini` next to the exe: `webhook_url=...` and `interval_seconds=`. tools\out is git-ignored; never commit the webhook URL.
`lobbytool.exe watch dry` prints the messages instead of posting (the console shows emoji as mojibake; the webhook body is UTF-8).
Uses the eos.ini credentials next to the exe like the other lobbytool modes.

## Discord announcements from every player's game (discord_webhook=)
pdnet.ini `discord_webhook=<URL>` (src/discord.*): the PC that owns a public SearchableGame lobby posts to the webhook
on a worker thread: opened (mpsd Write, created + owner), player count / full (OnLobbyEvent -> LobbyChanged),
closed (Write leave when last member -> LobbyLeft; EOS Closed/Kicked -> LobbyLost). Host migration: the old owner stops,
the new owner adopts the lobby silently and announces later changes. A crashed/killed host posts no "closed".
The stage C kit pdnet.ini has `discord_webhook=`; make-testkit.ps1 fills it from -Webhook or pdnet\tools\out\lobbywatch.ini
(-Webhook none = off). The URL is then inside every kit: if abused, delete the webhook in Discord and rebuild kits.
Don't also run `lobbytool watch` on the same channel, or lobbies are announced twice.

Discord messages: 'opened' is posted once the lobby has the game's search handle and names gametype (0 Battle Royale, 1 Tag Team, 2 One-on-One) and gamerule (0 Standard Battle ... 11 Annihilate the Odd One, order from Assets\Data\Menu\livesession_en.ssb); no rating. Confirmed in game with build-discord2 (7C6381D2), installed in the Direct build.

Discord toggles (pdnet.ini, default on): discord_lobby_messages (opened/closed), discord_player_messages (<name> joined/left/full). Join/leave messages name players from each member's PD_NAME.

"Closed" on game exit (2026-10-27): when the game exits while alone in its lobby, pdnet writes `discord-close.txt` and pdlaunch edits the message to "closed". Both now skip this when discord_lobby_messages is off (before, it still happened whenever player posts were on). Fix ab55850 on master, c081b28 on kit27; testers need the new pdnet.dll and pdlaunch.exe.

Options > Sound row (2026-10-27, works): Aura Particle Beeps (On / Off, settings.ini aura_beeps) under Music / Sound Effects Volume. A Menu Sounds row was tried and dropped (the cursor sounds come from the game's own code at 0x14024B456 / 0x14024B118, not only from menu cmd g1 #43). The SOUND window (menu 4 option, window 4, built at 0x01350 of option_en.ssb = the dump's option_enx.ssb) gets the rows from HookedAddItem; its key handler (0x02AF4) and row drawing use option 4 + cursor row via menu cmd 66 / 67, so option 6 is pdnet's while the script state is SOUN (menu cmd 12 / 6). Every sound starts in FUN_140240870 (2D) or FUN_140240a00 (3D): bank edx, cue r8d, sound r9d; both inline-hooked (SoundOff). sound_log=on logs every sound once, with its caller (for 3D sounds through FUN_140276330, that function's caller). Aura beep: bank 1 sounds come from the stage's sound emitters (FUN_1402214c0: mode 0 replays its cue on a timer, mode 1 starts a loop once). Muting 3D bank 1 cue 4 (the loop, Experimental 34) or cue 1 (the timed one, Experimental 35) did not stop the beep. The beep is the aura counter: FUN_14024de20 raises a player's aura and plays 2D bank 0 cue 5 each time the whole number goes up (own player only; call at 0x14024DF22). Experimental 36 mutes exactly that call when aura_beeps=off: confirmed in game by the user (2026-10-27, the beeping stopped). 3D bank 0 cues 34 / 35 are footsteps (FUN_140231010 / FUN_1402311e0).

Texts (2026-10-28, Experimental 50): the build label now reads "pdrebirth <kit>" (was "pdnet <kit>"; "Rebirth <kit>" from Experimental 52); the Settings screen's Voice Mode hint is shorter ("Modern: Mic mutes only you. Classic: Off mutes everyone."); every menu text saying Xbox Live says Online (RenameXboxLive, run on every menu script by the MenuTextPatches watcher, now always started): phrases first ("Go to Xbox Live" -> "Go online", "Use Xbox Live to connect" -> "Connect", "signed out of Xbox Live" -> "signed out", ...), then "Xbox Live" -> "Online", "XBOX LIVE" -> "ONLINE"; all shorter, written in place. kMenuItems / kSettingsTexts also accept the "Online Options" / "ONLINE OPTIONS" spellings (still renamed Settings). The Help item's "Copy your Xbox Live save" stays (it is the Xbox save). Experimental 51: the multiplayer menu types its header out on its first visit, before the 50 ms watcher renamed it ("Xbox Live_"), so RenameXboxLive also runs in an inline hook on FUN_1402aa620 (prepares each script right after it is read, before it runs; starts `test rcx,rcx / jz 0x1402aa7d8`, replayed with an absolute jump).

Players removed with the admin tool (2026-10-28, Experimental 49): the admin tool (outside the repo) zeroes every stat of a removed player, the PlayerName stats included, but EOS keeps a 0 entry on each board, which the game showed as "Player 0002dc" with 0. scores.cpp Ranks now leaves out entries with score 0 and no name stats at all (a real player always has a name: pdnet sends it every session, so a removed player who plays again shows up, named, as normal).

Discord "closed" after a host hand-over (2026-10-28, Experimental 48): when the host left with others still in, the player who took the lobby over only marked the message closed if their OWN lobby-posts setting was on, so with it off the message stayed "Open". Since Experimental 41 a message exists only if the original host posted "opened", so "closed" (LobbyLeft, LobbyGone, the exit note and pdlaunch's CloseDiscordMessage) no longer checks this PC's setting: it edits the message whenever there is one, and Send still drops it when there isn't.

Joins that hang (2026-10-28, Experimental 47): logs from SnowRyze, hammy and toaf showed the same thing: the joiner joins the lobby, voice works, but its join requests (p2p control channel) get no answer for 50 s or never, until EOS's link comes up (then the host answers within 250 ms). The host only accepted a connection when Epic's "wants to connect" notice reached it (OnRequest), and that notice travels over Epic's notification socket, which toaf's log showed dropping ("Client didn't send heartbeat"). Now every lobby member's connection is accepted as soon as they show up in the lobby (mpsd OnLobbyEvent -> p2p::Expect: EOS_P2P_AcceptConnection on the EOS thread, plus a ping from this side so the connection is opened from both ends; once per player per minute). Log: "accepted ahead of its request", "reached from this side".

Discord lobby message layout (2026-10-27, Experimental 45, from the community's spec): one message, edited in place:
  🌐 3/4 Open Lobby - Rated Standard Tag - toaf joined the lobby
  AnoMayCry / Snow Ryze / toaf / < public slot > (one line per slot: host first, then join order)
"Full Lobby" at 4/4; first event "<host> opened the lobby"; closed: "🔴 Closed Lobby - <match> - <host>'s lobby closed". ShortMatch: rated, rule, type in short terms (BR / Tag / 1v1; Standard, Ante, Quick, Puny, Heavy, Spec Enviro, 1 School, Highlander, Puny Only, Heavy Only, KO, Odd One Out). Plain text (Experimental 46: the user didn't want 45's code spans); names markdown-escaped (Plain -> Name). The webhook gets the real 🌐, not ":globe_with_meridians:" (webhooks don't convert shortcodes). lobbytool watch still uses MatchDescription.

Scoreboard entry hidden (2026-10-27, Experimental 44): the user asked to remove "phantomdust.online" from the leaderboard. A game client can't delete another player's EOS stats (only the Epic Developer Portal / server tools can), so scores.cpp Ranks drops entries whose name matches kHidden (case-insensitive; never this PC's own), moves the ranks below up, and logs the hidden product user id ("is off the boards") so it can be matched by id later. Players on older kits still see it.

Player identity kept outside the game (2026-10-27, Experimental 43): the scoreboards (EOS Stats / Leaderboards) count the EOS product user, which comes from the device-ID credential pdnet keeps in LocalState\eos\credentials\*.cred (credstore.cpp: the sandbox can't use Credential Manager), next to LocalState\pdnet\identity.txt. Uninstalling the game deletes LocalState, so a reinstall signed in as a new Epic user (a duplicate on the boards). pdlaunch (KeepIdentity) now copies each of those files, as they are (DPAPI-protected), into Windows Credential Manager (generic "pdnet/identity/<file>", CRED_PERSIST_LOCAL_MACHINE, this Windows user) the first time it sees them; before every game start it puts a missing or different file back, so the first identity kept is the player's for good. Lost only with the Windows user profile (or by deleting those entries in Credential Manager).

Start-up volume in matches (2026-10-27, Experimental 42): a match has no menu (CurrentMenu -1), which SignalPastTitle counted as the intro, so matches played at splash_volume. "No menu" now counts as the intro only before the player is first past the title.

Discord, no posts for an unannounced lobby (2026-10-27, Experimental 41): a host with lobby posts off still got a "closed" message, posted by another player's game after EOS handed it the lobby (host migration adopts the lobby as announced; with no PD_DISCORD message id its "closed" became a new POST). Now only the "lobby opened" job (Job::opening) may create a message; player posts and "closed" only PATCH an existing one (else logged "has no message (not announced) - nothing posted"). Every player needs Experimental 41+ for this.

Controllers (2026-10-27, Experimental 40, untested beyond Xbox): the game reads pads only through Windows.Gaming.Input.Gamepad (factory IGamepadStatics fetched once, FUN_1400deb80; no XInput / DirectInput), which lists Xbox and XInput pads only. pdnet answers that class (hooks.cpp -> gamepads.cpp PadFactory): Gamepads = the OS list + one VirtualPad per pad in LocalState\pdnet\pads.bin (padshare.h; seqlock, heartbeat), GamepadAdded/Removed for those too, Vibration written back. pdlaunch's ReadControllers thread loads SDL3.dll (3.4.18, next to pdlaunch, from C:\Users\ryhol\tools\downloads\sdl3; build-pdlaunch.cmd copies it to tools\out, make-testkit ships it) and writes non-Xbox pads every 4 ms in Xbox layout (SDL south/east/west/north = A/B/X/Y; Y axes flipped), rumbles them from the game's Vibration. Xbox types (SDL XBOX360/XBOXONE, incl. XInput off-brands) are skipped; SDL hints: HIDAPI_XBOX off, WGI off, background events on. pdnet.ini more_controllers=off turns both sides off.

Kit layout (2026-10-27, from Experimental 39): Setup.bat alone at the top of the zip, everything else in a pdnet\ folder (README, the other .bat/.ps1 files, pdlaunch.exe, pdnet.dll, pdnet.ini, eos.ini, EOS SDK, xaudio2_9redist.dll, kit-version.txt). Setup.bat runs pdnet\setup.ps1; every script uses its own folder ($PSScriptRoot / %~dp0) and pdlaunch / pdnet look next to themselves, so the debugger command line and the icacls grant point at the pdnet folder. make-testkit.ps1 writes the zip entry by entry with forward-slash names (Compress-Archive and ZipFile.CreateFromDirectory on .NET Framework write backslashes), reads kit-version.txt from pdnet/ (or the top, older kits), and takes -Commit <kit27 commit> for the Source line. The active kit folder (pdnet.log "config: reading") is now <kit>\pdnet.

Clock check (2026-10-29, Experimental 59-64): the MSI tester PC (clock about 31 h fast) and the user's desktop connected (direct and through Epic's relay) but never passed a packet; with the clock corrected they joined at once. EOS's encrypted P2P link seems to fail when two PCs' clocks are more than about a day apart. From 64 the Windows clock is not changed at all: pdlaunch, before injecting pdnet, reads the Date header of a HEAD to https://api.epicgames.dev/ (certificate date errors ignored; 3 s timeouts) and writes the seconds this PC is behind into LocalState\pdnet\clock-offset.txt (deleted at each start; clock_check=off: none). pdnet's eossdk::Load then calls CorrectEosClock (src\eosclock.cpp): if the offset is a minute or more, the EOS SDK module's imports GetSystemTimeAsFileTime, GetSystemTime, GetLocalTime (kernel32), _time64 and _ftime64 (api-ms-win-crt-time) return the corrected time, in that DLL only. Logged as "clock: ..." in both logs. Not yet tried in a match with a wrong clock. History: 59/60 asked with a message box and 61 with an admin prompt from pdlaunch, all of which opened behind the full-screen game or only flashed in the taskbar (a background process can't take the foreground); 62/63 had Setup.bat add a SYSTEM scheduled task "Phantom Dust clock" (62 as an -EncodedCommand, which Windows never started: an encoded PowerShell command making a web request is held, probably by Defender's script scanning; 63 as C:\Program Files\Phantom Dust pdnet\clock.ps1). The user asked for no Setup step, so the task is gone from Setup; Remove.bat still deletes the task and that folder.

Options > Send Log (2026-10-29, Experimental 56; item shown from 57, picking works from 58): a seventh item in the main menu's OPTIONS list (option_enx.ssb, after Keyboard). LogItems adds it and its hint through HookedAddItem with no menu check: the list is built before menu 4 counts as current, which kept the item out in 56. Only the Options scripts add $KEYBOARD items, and no script has an aMAIN_S6 label. Picking item N: each frame the script passes its state ("aMAIN") to menu command 6, which writes the label to run for that frame's input at manager +0x1062c (FUN_140245f00: state + "_S<N>" for input event 100 + N at +0x10414). There is no aMAIN_S6, so the game does nothing; HookedSetState (the cmd 6 wrapper) sees event 106 and that name in menu 4 and runs discord::SendLogs on a thread (3 s debounce): one multipart POST to log_webhook (new pdnet.ini key) with "Log from <name> - <kit>", the last 8 MB of LocalState\pdnet.log and the last 1 MB of the kit's pdlaunch.log. The item's text shows Sending Log... / Log Sent / Log Not Sent: <reason>. Experimental 57 hooked FUN_1402ab6b0 instead, which was wrong: it takes (vm, name) and menu picks never go through it. The kit gets log_webhook from log_webhook_url= in the untracked pdnet\tools\out\lobbywatch.ini.

Relay fallback (2026-10-29, Experimental 55): a log from the user's MSI PC showed one player (0002c46a, 10.0.0.180) never reachable from it although EOS reported the connection "direct" (pings unanswered, joins timing out both ways), while p2p_relay=force had fixed a similar join before. With p2p_relay=allow (the default), p2p::NoAnswer now switches EOS to ForceRelays for the rest of the game run and closes + re-accepts that player's connection (relay settings only apply to a connection made anew), when: the host's lobby ping (Expect) gets no answer in 8 s, a Request (join) gets none in 6 s, or a link answers none of its last 4+ pings. Not for connections already relayed; once per player per minute. Logged as "... - reconnecting through Epic's relay" / "relays forced for the rest of this game run". p2p_relay=force still forces from the start; off disables both.

Stable 8 (2026-10-28): Experimental 54 promoted by the user; tag stable-8 = kit27 d27fc7d (pdnet C7AB4121), on GitHub. Since Stable 7: Rated / Unrated in the lobby's SESSION INFO box, no Nametag Colours on/off row.

Nametag colours toggle removed (2026-10-28, Experimental 54): the Settings row "Tag Colours" (nametag_colors on/off) is gone, from the config, the kit ini and the README too; nametags always use the three colour settings (White is one of the choices). An old nametag_colors line in an ini is ignored.

Lobby SESSION INFO rating (2026-10-28, Experimental 53): in the lobby (menu 44) the SESSION INFO box's "Game Rule: <rule>" line now reads "<rule> (Rated)" / "(Unrated)", like the Lobby Browser's (HookedDrawText / RatedRuleText; the lobby's value from scores::RatedMatch, the browser's from the search results).

Rebirth title (2026-10-28, master only, not in tester kits): the word is now the user's own lettering artwork (pdnet\tools\rebirth-word\rebirth-art.png; make.ps1 keys out its painted checkerboard and writes src\rebirth_word.h, 155x60). Squish fixed: the title script's draw sub ("dTitle") calls g6 #34/#35; #35 (FUN_140279e70) draws into a 592x456 offscreen target, logo_02 in FUN_140279640 at x 47..607, y 153..249 and the glow logo_03 in FUN_140278360 at y 139..251, one texture row per pixel, the edges float constants at 0x1402f80a8 / 0x1402f810c / 0x1402f8090 / 0x1402f8114 (each read only there). TallerTitleBoxes moves them out by kSubH (74) and moves "PRESS ENTER TO START" (FUN_140279410, y 379..395 at 0x1402f8168 / 0x1402f8170, read only there) down 60 (the most: the offscreen target is 456 rows), once the FDLogo copy is made, so the taller textures draw unsquashed. The glow box starts 14 rows above the logo's, so the word's glow now goes at glow row kSubH + 102 - kLift (was + 96, 6 rows high). The word starts kLift (10) rows up, blended over the logo texture's last rows, to sit closer to the logo. Waiting for the user to check it in game. 2026-10-30: new artwork from the user, "Rebirth" with spiked bars like the logo's own (rebirth-art.png, 560x96 with real transparency; make.ps1 now only crops it, to 375x88, no keying or scaling), centered under the logo (the user's choice over its left-aligned place in the canvas); kSubH 82 with kLift 10 gives the 88-row word 2 spare rows, as before. Still too close to PRESS ENTER (already as low as the 456-row target allows), so kLogoUp (24) moves the logo and glow boxes, and with them the word, up; the flames behind stay.

Stable 7 (2026-10-28): Experimental 52 promoted by the user; tag stable-7 = kit27 572a588 (pdnet 389EDD2F), on GitHub. Since Stable 6: PlayStation / Nintendo / other controllers (SDL3 via pdlaunch), player identity kept in Windows Credential Manager, normal volume in matches, Discord fixes (no posts for unannounced lobbies, closed after a hand-over) and the community message layout, joins accepted ahead of Epic's notice, removed players off the boards, Online instead of Xbox Live, "Rebirth" build label, Aura Particle Beeps setting.

Stable 6 (2026-10-27): Experimental 38 promoted by the user; tag stable-6 = kit27 dae8c87 (pdnet EED1A071, pdlaunch from the same day), on GitHub.

Start-up volume (2026-10-27): splash_seconds is gone (setting, Settings row "Logo Time", kit ini). splash_volume now applies while the title screen or intro is on: pdnet (SignalPastTitle) keeps LocalState\pdnet\past-title.txt while any menu other than 7 (playdatasel, the title) / 80 is on, and deletes it when the player backs out to the title (Experimental 38; 37 only did the first time). pdlaunch (which deletes the file before resuming the game) uses the start-up level while it is missing (before it first appears at most 3 minutes; not at all without pdnet), and remembers Volume Mixer changes only at the normal level.

Game volume (2026-10-27): Windows never saves a Volume Mixer level for PDUWP.exe (no entry in its PolicyConfig\PropertyStore), so it starts at 100% every time. pdlaunch's WatchVolume remembers it, but used to watch only the first audio session it found and saved volume.txt per kit folder, so nothing was ever saved. Now it re-reads all of the game's sessions every 250 ms, gives any new session the remembered level, and saves to LocalState\pdnet\volume.txt (shared by every kit; the kit folder's volume.txt is still read as a fallback).

Empty lobbies (2026-10-27): after a host's game closes or crashes, Epic keeps the lobby for a while with 0 members, and the game listed it as 0/4. Only its owner can destroy it, so lobby searches and Quick Join now skip any lobby with 0 members (MultiplayerService.cpp, GetSearchHandlesAsyncImpl).

## Stage / variant selection (RE notes, PDUWP Ghidra addresses)
- Stage id = DAT_1403e0304 (0-29, names in Assets\Data\setup\stageset\stageset*.dat; files Map\stNN.alr/.dat; e.g. 02 Spiral / 12 Lightning / 22 Sunlight Highway). Units digit = arena (camera st%02dcam.alr uses id % 10).
- Online: the host's stage menu picks a stage-set entry DAT_14051775c (FUN_140213c00); the table at DAT_140509bb8 (40-byte entries, filled at run time) gives [0] stage id and [1] layout count; the host rolls DAT_140527c54 = rand() % count (rolled again for each next round in FUN_140187b70).
- That layout index (DAT_1404c51f0 / byte DAT_140531dcf) selects a section inside stNN.dat: header uint32 table, slot index+1 (falls back to slot 0 if FFFFFFFF), FUN_140203720. Arena 8 (Zain) forces 0.
- Sync: not in the session or lobby. The host (slot 0) puts stage id + layout (+ 1405277b4/1405277fc/14051ede0/14050e13c) in its 0x80-byte peer message (FUN_1401910a0 -> FUN_140125430); clients copy them only from slot 0 (FUN_140191a60, "Recv Name %s"). The search handle's stage=3 is unrelated (always 3).

Nametags on dead bodies follow the lobby's game host: the patch is now a jump at 0x1401511be to a cave that skips the health check while a pdnet flag byte is 1 (gamepatches::SetNametagsOnBodies). Each member publishes PD_BODYTAGS (its nametags_on_bodies); mpsd ApplyHostRules uses the PD_HOST member's value on join, lobby events and pushes, and UseOwnSettings on leave/lost. Hosts with an older pdnet publish nothing, so each PC keeps its own setting. build-hostrules (929014AD).

Microsoft Store kit (stage C): `make-testkit.ps1 -Stage Store -EosIni .\eos.tester.ini -Build build-hostrules` -> D:\PhantomDust-pdnet-StageC-Store-TestKit-<md5>-with-eos.zip (testkit-store\: Setup/Remove/Collect-Log .bat, pdlaunch.exe, pdnet.dll, stage C pdnet.ini). store-setup refuses non-Store installs, enables the package debugger (pdlaunch) and asks for admin once for CheckNetIsolation LoopbackExempt (the Store manifest only has internetClient; pdnet's P2P links are sockets on this PC). Injected mode has never run in game; first Store test pending.

Play Dead (skill id 360, 'Health recovers while immobile'; also status effect 1 = -50 movement for 30 s) doesn't hide the nametag. Found with nametag_debug=deep: status record +0x4b4 = current skill/action id (360 while playing dead, 134 after being hit, 751 after a skill ends), +0x4ac = its phase (1 while lying, 3 when interrupted). The player object DAT_1404bc9d8[i] contains the status record at +0x40. The dead-colour routine now uses dead_nametag_color when g_bodyTags is on and (health == 0 or (+0x4b4 == 360 and +0x4ac == 1)). build-playdead.

Play Dead online: other players' records on this PC never show the lying pose (no field changes on lying down / getting up; object+0x12FD4/12FD8 48.0/49.0 is local-player only). Rule now: +0x4b4 == 360 and (+0x4ac == 1 [own player, exact] or status effect 1 value +0x61c8 < 0 [the skill's 30 s slow-down, ends at 30 s or next skill]). Colour routine uses rel32 jumps (256-byte cave). build-playdead5.

Play Dead colour, final rule (build-playdead6): TrackPlayDead thread (20 ms) sets g_playDead[slot] when status +0x4b4 becomes 360 (or the skill-use counter +0x4a4 moves on while it stays 360) and clears it when the player moves > 0.75 units from where they lay (position +0xa0/+0xa4/+0xa8 = object+0xE0.., baseline taken 1 s after lying down), takes damage (+0x0c drops), dies, or uses another skill. The colour routine reads g_playDead[r15] / [rbx]. Logs 'P<n> is playing dead' / 'stopped playing dead (<why>)'.
Confirmed online 2026-10-21: the Play Dead tag turns yellow while lying and white on moving or being hit (build-playdead6, 9E57B28E, installed in the Direct build).

Lobby Arsenal editor (build-lab1, lobby_arsenal_editor=on default): menus are scripts on a stack, manager DAT_140533c58 (+0x105f4 = current menu index), FUN_140246130(mgr, menu, force) = open on top, FUN_1402461f0() = back; script commands FUN_140207020/FUN_140207050 (command table 0x1403eefe0). Menu table DAT_1403fa3d0 (0x18 each): livelobby 44, laboratory 45, labostock 63, arsenaledit 64, livearsel 67. pdnet inline-hooks both (10-byte prologue, trampolines) and logs 'menu: open/back'. The invite button (TitleCallableUI::ShowGameInviteUIAsync) opens 64 while the current menu is 44; while opened from the lobby, the editor's exit to any other menu (except 63/64) is turned into a back. Untested in game when committed.
Confirmed in game 2026-10-21: Invite in the lobby opens the Arsenal editor and backing out returns to the lobby (build-lab1, DA15D5EE).
Confirmed in game: the lobby button reads 'Arsenal Lab' (build-lab2, 0715AFA6; newstrings table DAT_1405b0918 patched in memory, English only).

Lobby rule change (build-rule1, untested): the lobby's Gamertag option (TitleCallableUI::ShowProfileCardUI[ForUser]Async) opens liverulesel (48) for the host (slot DAT_140527bc4 == 0). Live values: game type DAT_14050d2d8, rule DAT_140518cf0 (FUN_140183010 = the menu's left/right with per-type rule limits). Confirm (48 pushing another menu) -> back to the lobby + publish: 1405277b4/1405277fc/140527ab8, property table DAT_14051ef80 (0x18 entries, id 3 type / 4 rule, value at +8), FUN_140283220() session update, FUN_140191210() resend of the host settings message (clients copy type/rule from slot 0 in FUN_140191a60). Back from 48 restores the saved values. Game type changes are refused. Open: client lobby display, arsenal re-check on clients.

Lobby settings change (build-rule6, 7AA5EF89) works solo 2026-10-22: the host's Change Stage opens liverulesel (48) with the stage list inside. Fixes found with menu_debug: the menu finishes at once if a stage is chosen (session cmd 41 reads DAT_14051775c; set -1 before opening), and its set-up cmd 80 (0x140213f00) resets type/rule (wrapped in the command table 0x1403f1150 to restore the lobby's values). Game type changes are allowed when the players fit (One-on-One 2, else 4; sets DAT_14051c0a4). Not yet tested with live players: clients' lobby text and battles, arsenal re-checks, and the EOS lobby's own member limit (still the old one; going from One-on-One to a 4-player type may not let more players join until it is updated).
Game type change also resizes the hosted Epic lobby (xsapi::SetHostedSessionsMaxMembers -> SessionStore::SetHostedMaxMembers, owner Push with the new maxMembers), on a background thread. build-rule7.

Live test 2026-10-21 (3 players): Tag Team -> Battle Royale worked; Battle Royale -> Tag Team crashed the other players' games (no client log: Collect-Log failed while the game held pdnet.log open). Changes into Tag Team are now refused (build-rule8). Collect-Log scripts copy the log with FileShare.ReadWrite.
Backing out of the settings menu runs session cmd 29 (wait) then cmd 190 (0x140216150 -> FUN_140181d70, abandon the session being created) and opens 36: from the lobby that tore the lobby down (endless loading). cmd 190 is now a no-op while the menu is opened from the lobby, and 48 -> 36 is a cancel (restore + back). build-rule9.

One kit for both versions, no game files changed (2026-10-22, after the Store tester confirmed injected mode works): make-testkit.ps1 default -Stage Kit packs testkit-store\ (setup.ps1/remove.ps1 + pdlaunch.exe + pdnet.dll) as D:\PhantomDust-pdnet-TestKit-<md5>-with-eos.zip. setup.ps1 handles the Store install and unpacked copies (registers an unpacked folder if not installed), and undoes older DLL-swap installs (restores from _pdnet_backup, or puts Microsoft.Xbox.Services.orig.dll back) before enabling the package debugger, the folder ACL and the loopback exemption. The DLL-swap kits stay available as -Stage C/B.

Direct build switched to the launcher setup 2026-10-21: original Microsoft.Xbox.Services.dll back in D:\PhantomDust-Direct, pdnet runs from the kit folder D:\PhantomDust-pdnet (pdnet.dll + pdnet.ini there; install a new build by copying it to D:\PhantomDust-pdnet\pdnet.dll). Confirmed in game: patches, Arsenal Lab, lobby settings change, Epic sign-in. setup.ps1 fix: nosuspend.ps1 -Off runs in its own process (both define the IPackageDebugSettings type). The One-on-One player check now counts pdnet's lobby members (xsapi::SharedSessionMembers); DAT_140594ea4 said 3 in a solo lobby. build-rule11.

Live test (1v1 -> BR, 3rd player joined): host fine, but player 2 (joined as 1v1) never saw player 3: its lobby kept DAT_14050d2d8 / DAT_14051c0a4 from joining; the host message only updates 1405277b4/1405277fc. FollowHostSettings thread: non-host in the lobby applies changes of the host's type/rule to its own type, rule, battle rule and player limit (only changes seen while in the lobby). build-rule12; all players need it.

Volume (pdlaunch, untested): pdlaunch now stays running until the game exits. It finds the game's audio session (IAudioSessionManager2, matched by process id), holds it at splash_volume% of the remembered level for splash_seconds, then the remembered level, and afterwards saves every Volume Mixer change to volume.txt next to pdlaunch.exe. Settings remember_volume / splash_volume / splash_seconds in pdnet.ini (kit folder).
Volume fix confirmed in game 2026-10-21 (quieter splash, Volume Mixer level remembered). Kit D:\PhantomDust-pdnet-TestKit-0CD1AD37-with-eos.zip rebuilt with it.

Reverted the lobby settings change (2026-10-21, user: 'isn't working well'): gamepatches.cpp/.h, config.cpp/.h, mpsd.cpp/.h and xsapi.h are back to cf94720 (Arsenal Lab + label rename, Play Dead, nametags kept). Kept: the one-kit launcher setup, pdlaunch volume fix, Collect-Log fix. The RE notes above (menu 48 / session commands 29, 41, 80, 190) stay for a later attempt. build-audio1 (E3E4A487).

Cloud save import (build-import1, 85B0685B): the first time the game asks for its saves (GetForUserAsync), pdnet copies every container/blob of the real Xbox cloud save (real GameSaveProvider for the game's user + SCID; pdnet's own WinRT calls aren't hooked) into LocalState\pdnet\saves, then writes saves\_cloud_import.txt with the outcome. Skipped if local saves exist; 20 s limit for Xbox to answer; read only. cloud_save_import=on|off. Not yet tested against a real cloud save (needs a Store player signed in to Xbox with no pdnet saves). Also: the launcher was pointing at make-testkit's staging folder (Setup.bat run from it); re-pointed to D:\PhantomDust-pdnet, and make-testkit now deletes the staging folder after zipping.

Xbox save import moved to the old Help & Support button (2026-10-21; no automatic import, cloud_save_import setting removed). The game opens http://xbox.com/phantom through Windows.System.Launcher (ILauncherStatics, activated via the hooked RoGetActivationFactory); pdnet answers with a stand-in (gamesave.cpp LauncherFactory) that starts the import for xbox.com addresses and forwards everything else. Labels help_main / help / help_hint renamed in the string table (RenameStrings, also the Arsenal Lab ones). First try on the unpacked Direct copy: the real GameSaveProvider::GetForUserAsync hung outright (no Xbox sign-in) after the live saves had been moved aside; saves restored by hand (+ byte copy in D:\PhantomDust-pdnet\saves-safety-copy-20261021). Now: the Xbox read runs on its own thread into LocalState\pdnet\saves.xbox-import, 90 s limit, and only a complete read swaps folders (live -> saves.before-xbox-import-<date>, import -> saves); game writes are paused after a successful swap until restart; Windows toast on success/failure. build-import3. Still untested against a real cloud save (needs a Store player signed in to Xbox).
build-import3 test on the unpacked Direct copy: the import started, the real Windows.Gaming.XboxLive.Storage.dll loaded (and pdnet IAT-hooked it), no result was ever logged, and the game later crashed in its deck code (PDUWP+0x15aa15, FUN_14015a9d0, bad card index); saves untouched. build-import4: the import refuses on non-Store packages (notification), and the hooks skip Windows.Gaming.XboxLive.Storage.dll. Unconfirmed whether the crash was caused by the import.
Import fallback (build-import5): unpacked copies read the local Xbox save cache LocalState\..\SystemAppData\wgs (containers.index + <folder GUID>\container.<n> + blob files named by GUID, Windows GUID byte order, fields checked and logged); the Store version tries the Xbox service first and falls back to the cache. Cache copies go to saves.cache-import, then the same swap. Untested on a real cache (the Direct PC's wgs is empty).

## Open items (parked 2026-10-21, no testers available)
- Xbox save import (build-import5 / kit 3F8C59F0): failure path confirmed on the unpacked Direct copy; a real import is untested. Needs a tester who played the Store version on that PC: press IMPORT XBOX SAVE, restart, check story/collection/arsenals, send Collect-Log (it lists every container/blob or where the wgs cache parse stopped).
- In-lobby game type/rule change: reverted (notes above for a later attempt).
- Tag Team switching crash, client-side lobby limit: only relevant if the lobby settings change comes back.

Loading watchdog (build-watch1, loading_watchdog=on default, observe only): stubs on both script command tables (menu 0x1403eefe0 x136, session 0x1403f1150 x196) report each command's result; a command polled non-stop (gaps < 1 s) with the same result for 30 s on one menu is logged once ('watchdog: ... waiting on it?'), plus 'menu N has been on screen for 60 s'. Normal screens log a few constant getters; a hang adds the stuck one. Step 2 (time out those specific waits) once hang logs exist.
Watchdog baseline (normal play, 35 menu changes incl. a lobby): only the title/start screen (menu 7) idle logged menu commands 6 and 109 = 1 for 30 s+ - normal, not a hang. Everything else quiet.

## Tester kit names (from 2026-10-22)
Kits are `D:\PhantomDust-pdnet Experimental <N>.zip` (make-testkit.ps1 numbers them; kit-version.txt inside says the
build and settings). A kit becomes `Stable <M>` only when the owner says so: `make-testkit.ps1 -Promote <N>`.
Per-kit settings: `-Set voice_chat=off,p2p_debug=on`; `-Notes "..."` for kit-version.txt.
Stable 1 = build-hook2 (AFCEAD9F) with voice_chat=off, lobby_gamertag_option=off (a join worked).
Stable 2 = build-icons7 (F2117D66, was Experimental 7): megaphone mic indicator, nametag colours, join retry,
lobbytool left out of sessions, Setup name prompt fix (promoted 2026-10-22). kit-counter.txt keeps the last Experimental number.
Stable 3 = build-vmode1 (92539ABD, was Experimental 10): Stable 2 plus voice_mode (mic/classic) and the cleaned-up
pdnet.ini text (promoted 2026-10-22). Built from commit 443e445. To go back to the most stable version, use the highest Stable number.
Stable 4 = build-wins3 (375800B9, was Experimental 25, tag stable-4 -> 1ba798b, promoted 2026-10-24): EOS scoreboard
(4 global boards, names, Total Wins), Rated / Unrated lobbies (+1 / -1 minimum per match), shop top-up, Lobby Browser
name, ghost lobbies hidden, shorter join wait, Discord Rated/Unrated.
Experimental 1 = build-resend1 (70A91FE2): join requests re-sent every 2 s until answered (an intermittent "hung on
joining": the single request sent while EOS set up the P2P link was lost); voice + Gamertag on, p2p_debug=on.

## Waiting for testers (as of 2026-10-22; newest kit Experimental 15 has all of these)
1. **Nametag of a player who left a match** (Experimental 13): 3+ players, one leaves mid-match (disconnect / quit to
   menu / close the game); their nametag should vanish. Log: "gamepatch: X left the match - nametag hidden". Risk: if
   the game's peer list (0x140595ea8) doesn't list every player in a match, present players would be hidden too.
2. **Tag Team voice channels** (Experimental 11+): lobby everyone hears everyone; Tag Team match only your team. Log:
   "gamepatch: game state N (match)" (verifies the lobby/match detection), "voice: team channel: X muted".
   Also try the pause menu's Voice Chat option mid-match (assumed to be the same setting flag 15).
3. **Nametag colours in a real match** (Experimental 3+): enemy red everywhere, ally blue + dead yellow in Tag Team.
4. **Other players' megaphone** following their Mic setting (Experimental 5+; only this PC's own was seen).
5. **Arsenal Lab for non-hosts** (Experimental 14): a player who joined (not the host) sees "Arsenal Lab" (Y) in the
   lobby's hint line and Y opens the Arsenal editor, backing out returns to the lobby. Log: "gamepatch: lobby Arsenal
   Lab button for every player (1 of 1 check)" when a lobby opens (livelobby albmain_ty's host check: its jz +3 ->
   jz +0), "text 'lobby_client' now has the Arsenal Lab button" (seen on this PC already).
6. **Quick Join** (Experimental 15; was Quick Match): with a tester hosting a NON-Quick lobby (e.g. Standard), Quick
   Join should join it. A player with no usable multiplayer arsenal should only find Quick lobbies. Log: "Quick Join -
   any game type; rules this player can play: mask 0x..." (bit N = rule N; this PC: 0xDFF, no Heavy Only arsenal).
   The rename and the search change were checked on this PC with no lobbies open.

## Feature backlog (could be added / revisited)
Parked ideas and half-finished features, newest first. Each has its notes earlier in this file.
1. **Loading-screen timeout** (parked 2026-10-21). Step 1 was built and tested (commit 4cf65cb, observe-only "loading watchdog":
   stubs on the menu 0x1403eefe0 x136 and session 0x1403f1150 x196 command tables log a command polled with the same result
   for 30 s and screens up for 60 s; normal baseline = title screen menu 7, commands 6/109). Taken out of the build by request.
   Next: bring it back (git show 4cf65cb), catch real hangs in the log, then time out those specific waits (step 2).
2. **Xbox save import, real test** (built, kit 3F8C59F0+): IMPORT XBOX SAVE (was Help & Support). Failure path confirmed on an
   unpacked copy; a real import from the Xbox service (Store) or the local wgs cache needs a tester who played the Store version.
3. **In-lobby game type / rule change** (reverted 2026-10-21, commit 4ff540e). Worked solo; open problems: switching into Tag
   Team crashed other players; players who joined a One-on-One kept a 2-player limit (follow-host fix untested). Notes: menu 48,
   session commands 29 / 41 / 80 / 190, settings message 1405277b4/1405277fc.
4. **Lobby's Gamertag option for future settings** - DONE 2026-10-21 (build-gt5, tested alone): enabled even when alone
   (lobby_gamertag_option=on). Alone, it opens an empty player list; the window is ready for future settings.
5. **Spectator mode for tournaments** - feasibility only: camera/HUD switching via the local-player index DAT_1404bbe00 looks
   testable offline; a non-playing seat is hard (4 fixed player slots). Suggested order: camera/HUD test, spectate-after-
   elimination, then a spare-slot spectator.
6. **Tag Team lobby crash / team data** - only matters if 3 comes back.
7. **Voice chat over EOS** (traced 2026-10-21, plan below, nothing built).

Voice chat (backlog 7): the game uses GameChat 1 (`Microsoft.Xbox.GameChat.dll` + `ChatAudio.dll`, in-package). It is not
behind XSAPI: ChatAudio gets Xbox tokens itself (WebAuthenticationCoreManager) and talks to the Xbox chat service over a
MessageWebSocket, so it can't work without a real Xbox sign-in. Game side: wrapper `Xb1ChatManager` = `*(DAT_1404bbc48+0x18)`
(+0x10 the GameChat ChatManager, +8 its lock). FUN_1400c5e20 creates the manager and registers the outgoing-packet (+0x58),
compare-console (+0x68) and +0x88 handlers; FUN_1400c6850 joins = AddLocalUserToChatChannelAsync(channel 0, user XUID from
`*(DAT_1404bbc48+0x28)+0xd0`); the JoinChatAsync/LeaveChatAsync errors are caught and logged (game carries on). Mesh console
added/removed -> FUN_1400fc400 / FUN_1400fc380 -> HandleNewRemoteConsole / RemoveRemoteConsoleAsync. Packets ride the game's
mesh as message type 3 (CHAT). Lobby option 4 "Voice Chat: On/Off" (`<MUTE TOGGLE>`, livelobby ahlmenu_s3) toggles settings
flag 15 = byte DAT_1404bad34 (script group 10 #3 get 0x1400bde50 / #4 set 0x1400bdfc0); 1 = voice OFF: FUN_1400c64d0 calls
MuteAll/UnmuteAll and FUN_1400c61f0 (outgoing packet handler) drops mic packets. FUN_140026150 (the game's debug print) is
empty in this build. pdnet today: EOS platform created without RTCOptions (eossdk.cpp) and lobbies with bEnableRTCRoom =
EOS_FALSE (lobbies.cpp), so EOS voice is off at both levels. SDK 1.19.2.1 has eos_rtc*.h.
Plan: (1) RTCOptions on the platform + bEnableRTCRoom on lobby create (joiners get the room automatically), log
RTC room connection changes, new pdnet.ini voice_chat=on; (2) follow the game's own Voice Chat On/Off (poll DAT_1404bad34 ->
EOS_RTCAudio_UpdateSending) so the existing lobby option works; (3) leave GameChat failing quietly (no double audio), optional
stub later; (4) check whether the EOS lobby lives through the match (mpsd.cpp leaves it when the MPSD member leaves) - if
not, keep it open or move voice to a room of its own; (5) later: per-player mute (Gamertag window), volume. Needs: the
tester client policy in the EOS dev portal allowing Voice/RTC, and Windows mic permission for Phantom Dust (manifest
already declares microphone).

Lobby Gamertag option (backlog 4): greyed (with Kick Player) when nobody else is in the lobby - not an Xbox privilege (CheckGamingPrivilege* never called). The lobby script asks session command 28 (0x140213a80: 1 if the slot is this PC or found in the peer list DAT_140595ea8/0x298 entries) for each slot; Gamertag and Kick share that. menu_debug=on (off by default) logs each distinct lobby script command+arg+result once while menu 44 is up. Fix (gamepatches.cpp LobbyGamertagOption): livelobby.ssb's player-count function (file 0x3634) gates Gamertag's draw (`count < 2` -> grey, 0x4608) and its host/client select handlers (`count > 1`, 0x42B0 / 0x446C -> Gamertag window at 0x3518). A watcher thread patches each loaded copy (menu table entry 44 +0x10) to `count < 0` / `count > -1`; Kick Player (0x46B4 / 0x4320, same pattern, its window titled "Kick Player") is left alone. ssb format: header +0 code base (0x20 in the file, reused at run time), +4 labels, +8 labels end, +12 strings (file offsets; relative to their own field once loaded). Labels are 12 bytes: two base-40 words (table 0x1402f76a0: 0-9 a-z _) + code offset in words from the file start. A select handler label is `a<h|c>lmenu_s<item>` (host/client), a draw handler is `draw_menu`.

## Scoreboard

The original Xbox scoreboard had four global boards (table at 0x1403fb910, each with views 0-2): Rating
(leaderboard CurrentMPRanking, extra column TotalWin), BattleRoyaleWin, TagTeamWin and OneOnOneWin. pdnet
(scoreboard=on, src/scores.cpp, xsapi/stubs/Leaderboard.*) answers every query from the EOS leaderboard with the
game's name; friends views get the global board too.
- Rating: kept in progress.dat (0x1405b1370), reported as the Hero_MP_Ranking event (measurement Count) after each
  match; pdnet also sends it once per session when the scoreboard opens.
- Wins: the BattleRoyaleWin / TagTeamWin / OneOnOneWin events (no measurements) add 1 to that stat and to TotalWin,
  only while the save's multiplayer profile is active.

EOS Developer Portal, same deployment as eos.ini:
- Stats: CurrentMPRanking (Latest), TotalWin (Sum), BattleRoyaleWin (Sum), TagTeamWin (Sum), OneOnOneWin (Sum),
  PlayerName0 ... PlayerName8 (Latest): each player's display_name, 31 bits per stat (EOS leaderboards and EOS
  Connect keep no names for device-id accounts). The game asks for boards as Rating/BattleRoyal/TagTeam/OneOnOne
  and for row names through ProfileService.
- Leaderboards (descending, no end date): CurrentMPRanking on stat CurrentMPRanking (Latest); BattleRoyaleWin,
  TagTeamWin, OneOnOneWin each on the stat of the same name (Sum).
- Client policy (the tester client's): allow stats ingest for the local user and leaderboard queries.
Log lines start with 'scores:' and 'xsapi-pdnet: scoreboard'.

The scoreboard menu (liveranking, menu 41) shows only the 4 Global boards (PatchScoreboardMenu: items show $LEADERBOARD_3/6/9/12, their handlers store board 3/6/9/12); Friends/Favorites are gone.

## Rated / Unrated lobbies

rated_lobbies=on: the create-session Rating option (session number ratinglimit 0/1, originally Â±200 join limit) reads Unrated / Rated; the search option finds one kind; ratingmin/ratingmax are forced to 0/1000000 (no join limit). FUN_1401835c0 (rating += change, match start and end) is hooked: change 0 unless the session pdnet holds has ratinglimit 1 (scores::RatedMatch in xsapi/mpsd.cpp). The game's 'Official Battle rules only' checks (0x1401878c9 jg, 0x1401882c8 ja) are NOPed, so any rule can be Rated. The '[Official Battle] Applies toward Scoreboard Total' line is removed from rule descriptions in every menu script. Wins count in both kinds. Log: 'gamepatch: rating change'.

## In-game Settings

Multiplayer menu > Settings (was Xbox Live Options, liveoption menu 68). Its Options panel's Cross Play row is left out (g10 #3 flag 3 reads 0 while Settings opens or shows) and the Voice Chat row (and hint) is replaced through menu command 40 by pdnet's rows: Name (typed via CoreWindow CharacterReceived), Voice Mode, Body Tags, Tag/Dead/Ally/Foe colours, Logo Volume/Time, Discord Lobby/Join. Each row's text holds its value; left/right reaches pdnet through the script's empty case (0x05814 patched to session cmd 154, wrapped), with sound 37 (g1 #43). Row mapping patched at 0x0564C/0x05670/0x05674; window raised via menu cmd 37. Changes go to pdnet.ini (SaveSetting).
