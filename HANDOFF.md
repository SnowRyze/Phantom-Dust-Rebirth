# Handoff: Phantom Dust (2017 UWP): replacing Xbox Live / Teredo networking

Written 2026-10-17 by the first Claude Code session, then updated by a second session (source repair and test peer)
and again on 2026-10-18 to bring everything in line. Two sessions have worked here; see §10 and `git log` in `_netmod`.
The longer design notes and the research log are in [`MODDING_PLAN.md`](MODDING_PLAN.md). Read that next.

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
| `D:\PhantomDust-Teredo` | **Currently registered.** Original Microsoft networking (`pdnet.ini: network_mode=teredo`) plus local saves and the menu fix. DLL = original 12:26 stage 2 build (MD5 `E3E67884`; its networking code stays inactive in teredo mode). **The user plays online with this build and it works** (confirmed 2026-10-18). **Don't modify it.** |
| `D:\PhantomDust-Direct` | Same, plus `network_mode=direct`, `test_peer=on`, manifest capabilities `internetClientServer` and `privateNetworkClientServer`, and the newer test-peer DLL (MD5 `632CEAC6`, from `pdnet\build-testpeer`). Never tested with two PCs. |

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
- Build output folders: `build` / `build-stage2-1226` = original 12:26, `build-repair` = repaired source, `build-testpeer` = repaired + test peer (current source).
  Don't build into `build` or `build-stage2-1226`; they're the known-good copies.
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
2. **Run the test peer in the game** (§6a). This is the only end-to-end check possible on one PC. Switch with `builds.ps1 -Use Direct`,
   then switch back with `-Use Teredo` afterwards so the user can play.
3. **Two-PC test of stage 2** once the user has a second PC: LAN first, through an Xbox Live lobby (host creates it, the other joins).
   Read `pdnet.log` on both. The earlier question of whether the host needs the joiner's device address is answered at the mesh level in §6a
   (no: any inbound endpoint pair is accepted). It still needs confirming with real traffic.
4. **Internet play without port forwarding.** Not done; see §8.
5. **Stage 3b:** replace XSAPI multiplayer (MPSD sessions, SmartMatch, search handles, RTA), sign-in (`XboxLiveUser`) and title storage
   with the user's own server, implementing only the XSAPI surface listed in `MODDING_PLAN.md` §4b. Not started.
   Card packs and paid content: the agreed scope is restoring play, not unlocking paid content.
6. Optional: give the Direct build its own package identity once it no longer needs Xbox Live, so it can coexist with the Teredo build.

## 8. Difficulties encountered

- **Two responses were stopped by Anthropic's safety classifier**, and per its instruction they must not be reproduced:
  1. while writing the plug-and-play internet connectivity layer (automatic public-address discovery and NAT traversal on the game's socket);
  2. while writing the stage 3 relay server (`pdserver.py`), which forwards game traffic between players.

  I don't know the exact reason. The user was told that this session couldn't continue that part of the work. The partial files were
  removed in the repair (§6). The next session should know this happened, and talk it through with the user rather than recreating those pieces.
- Bash heredocs in this environment turned `\\` into `\` (and `\n` into real newlines) inside embedded Python. Use the Write/Edit tools for source edits.
- `WINRT_LEAN_AND_MEAN` strips the `produce<>` code needed to implement Windows interfaces. C++/WinRT coroutines that call `get_strong()` can't be `const`.
- Ghidra didn't define most of the UTF-16 log strings, so finding functions by string missed them. Finding them by import callers (`DumpCallers.java`) worked.
- An AppContainer can't reach loopback, so a server on this PC can't be reached from the game unless the *user* runs
  `CheckNetIsolation LoopbackExempt -a -n=Microsoft.MSEsper_8wekyb3d8bbwe` (a system setting; it's the user's call).
- Only one registration per package identity, which is why builds are switched instead of installed side by side.
- Inbound UDP to the game needs the `internetClientServer` / `privateNetworkClientServer` capabilities (added only to the Direct build).

## 9. User preferences learned

- Keep the Teredo build and the master dump playable and untouched; back things up before changing anything.
- Debug menu stays on Multiplayer → Split Screen.
- Server language: Python (the user had no preference). Testing will be on 2 PCs once available.
- They like short, practical explanations, with one-command scripts for anything they have to run.
- They actively play online with the Teredo build, so leave it registered when you're done testing.

## 10. Session history

- **Session 1** (2026-10-17, ~12:00–12:50): recon, the pdnet shim (saves, then stage 2 networking), menu move, build folders, this handoff.
  Two of its responses were stopped by the safety classifier (§8). It ended with the source tree broken.
- **Session 2** (2026-10-17, ~12:54–13:10): `git init` in `_netmod`, source repair, smoke test, test peer. Its commits carry the user's git name.
  `git log` shows the test-peer commit `8abdf07` first even though its author date (12:19) is earlier. Go by commit order, not the date.
- **2026-10-18:** the user played online with the Teredo build (log entry at 17:10). Session 1 updated this document to match git and the files on disk.
