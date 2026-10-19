# Plan: Epic Online Services (EOS) as the online backend

Goal: replace what Xbox Live provides (accounts, lobbies, matchmaking, and peer-to-peer connectivity through home routers)
with EOS, so players need no Microsoft account and no port forwarding, and a broken player-to-player link can fall back to
Epic's relays (fixing "cross lagging"). pdnet stays the integration point inside the game; there's no server of our own.

**The Teredo build (`D:\PhantomDust-Teredo`) is never touched by this work.** Everything happens in a separate build.

## Step 1 — feasibility test (throwaway)

Question: does the EOS SDK work **inside the game's sandbox** (UWP AppContainer)? EOS targets normal Windows programs.

Test build: a copy of the Direct build with `eos_test=on` in `pdnet.ini`. On game start, pdnet would, on a background thread and
without touching any game networking:

1. load `EOSSDK-Win64-Shipping.dll` from the build folder;
2. `EOS_Initialize`, then `EOS_Platform_Create` with the project credentials (cache folder = the game's LocalState);
3. log in with a device ID (`EOS_Connect_CreateDeviceId` + `EOS_Connect_Login`, so no Epic account is needed);
4. query the NAT type EOS sees (`EOS_P2P_QueryNATType`);
5. write each result to `pdnet.log` and stop.

Pass = steps 1–4 succeed in `pdnet.log`. Fail = we learn which step the sandbox blocks, before anything big is built.

## Step 2 onward (only if step 1 passes)

| Xbox Live piece the game uses | EOS replacement |
|---|---|
| `XboxLiveUser` sign-in, gamertag | EOS Connect (device ID), display name from `pdnet.ini` |
| MPSD sessions, search handles (lobby browser) | EOS Lobbies |
| SmartMatch (Quick Match) | EOS Lobby search by attributes |
| RTA session-change notifications | EOS lobby update notifications |
| Secure Device Association (Teredo links) | EOS P2P (NAT traversal, relay fallback) |
| Title storage (card data) | Ship the local `gsdata` files (already cached locally) |

Milestones: local sign-in → lobby create/list/join → two players connected in a match → Quick Match.

## Needed from the user (Epic Developer Portal: dev.epicgames.com/portal)

- A free Epic developer account and organization. **The user accepts Epic's terms; check that the EOS service agreement
  allows using EOS for a community project for a game you don't hold the rights to.**
- A product, with: **ProductId, SandboxId, DeploymentId** and a client with **ClientId + ClientSecret**
  (client policy allowing Connect, P2P and Lobbies).
- The **EOS C SDK** zip (download from the portal), saved locally.

## Step 1: PASSED (2026-10-19, 00:35 run)

`load=ok init=ok platform=ok deviceid=ok (already existed) login=ok (same product user as the previous launch) nat=ok (Moderate) lobby=ok`.
No threads were suspended while the game was in the background (nosuspend.ps1 works). Details of how we got there follow.

## Step 1 history

- load / init / platform: **ok** inside the sandbox.
- Device ID: first failed (`EOS_UnexpectedError`) because EOS keeps it in Windows Credential Manager, which the AppContainer
  can't use. Fixed with `credstore.cpp`, which points the EOS module's CredRead/Write/Delete/Free imports at DPAPI files in
  `LocalState\eos\credentials`. Now **ok**, and **login ok** (anonymous product user, no Epic account).
- NAT and lobby: not reached; Windows suspended the game (all threads `Suspended`) when it lost focus.
- **Suspension:** Store apps are suspended when minimized, which would freeze EOS (and Teredo) mid-match.
  `_netmod\nosuspend.ps1` turns on package debug mode (`IPackageDebugSettings::EnableDebugging`) so Windows doesn't suspend the game;
  `-Off` reverts it. A real release needs a player-friendly version of this (or ExtendedExecution, which the game already references).

## Step 1a: long-lived sign-in (built 2026-10-19, awaiting in-game run)

`eos=on` + `display_name=` in the Direct build's pdnet.ini. `eos.cpp` owns one EOS thread: device-ID sign-in at startup,
re-sign-in on auth expiry or logout (with backoff), `Post()` for later steps. Runner: `_netmod\eosrun.ps1 -Name ... / -Report`.

## Step 1b recon: how the game signs in (Ghidra, PDUWP.exe, 2026-10-19)

The game's user manager is a state machine, `FUN_1401017c0` (state at `+0xb8`, states 1–10):
- state 6: `FUN_140101fa0(mgr, Windows.System.User)` stores the Windows user at `+0x60` and moves to state 7. It then creates the
  `XboxLiveUser` through its factory (`FUN_140105370`, stored at `+0xd0`), then `XboxLiveContext(user)` (`FUN_1400edbe0`), and then
  `FUN_140104150` configures the context's settings and subscribes to `ServiceCallRouted` (debug logging).
- `FUN_140105200` / `FUN_1401052c0` add and remove the static `XboxLiveUser.SignOutCompleted` handler.
- state 10: reads the gamertag (`FUN_1400a6e30` → user vtable `+0x60` = `get_Gamertag`); if it's empty, it shows the literal "Gamertag".

XSAPI vtable slots (from the winmd, after IInspectable's 6):
- `XboxLiveUser`: 0x30 SignInAsync, 0x38 SignInSilentlyAsync, 0x40/0x48 GetTokenAndSignatureAsync, 0x50 …ArrayAsync,
  0x58 XboxUserId, 0x60 Gamertag, 0x68 AgeGroup, 0x70 Privileges, 0x78 IsSignedIn, 0x80 WebAccountId, 0x88 WindowsSystemUser.
- statics: 0x30 add_SignOutCompleted, 0x38 remove_SignOutCompleted.
- `XboxLiveContext`: 0x30 User, 0x38 ProfileService, 0x40 SocialService, … 0x70 MultiplayerService, … 0x98 TitleStorageService, … 0xc0 Settings, 0xc8 AppConfig.

**Consequence for the order of work.** XSAPI's own classes take an `XboxLiveUser`/`XboxLiveContext` and use their internals
(e.g. `MultiplayerSession(XboxLiveContext^)`, every service hangs off the context). So a pdnet-made user can't be handed to the real
XSAPI. Replacing sign-in means replacing, at the same time, every XSAPI service the game touches (multiplayer, matchmaking,
title storage, presence, social/profile, stats, leaderboards), with stubs where a feature isn't needed.
The user object itself is small (the 12 members above).

## Xbox-free mode (`xbox=off`, runner `_netmod\xboxfree.ps1`)

pdnet answers every `Microsoft.Xbox.Services.*` class itself (`pdnet\xsapi\`, generated from the winmd by `generate.py`;
unimplemented members log `xsapi-stub:` and return safe defaults).

- 2026-10-19: boots with no Xbox sign-in. Local user (name from `display_name`, id in `LocalState\pdnet\identity.txt`); title storage
  served from `Assets\Data(\gstorage)` (tms_options.csv, gsdata_en.dat); main menu, scenario mode and the multiplayer menu work.
- 2026-10-19, **stage A**: local in-memory sessions (`xsapi\mpsd.*`). **Create match works**: the game creates `SearchableGame/<guid>`
  (max 4), joins, writes it ~6 times, and sets up the PeerToHostUDP template.
  Calls seen in the lobby that stage B must back: `SetSearchHandleAsync` (MultiplayerSearchHandleRequest with Numbers/StringsMetadata),
  `SetActivityAsync`, `MultiplayerSession::SearchHandleId`, `DateOfNextTimer`, `CompareMultiplayerSessions`.
- **Stage B (next):** back the session store with EOS lobbies. Session ↔ lobby (members ↔ lobby members with attributes: id, name,
  device address, custom properties), search handle ↔ searchable lobby attributes, `GetSearchHandlesAsync` ↔ EOS lobby search,
  EOS lobby updates → `MultiplayerSessionChanged`, and `CompareMultiplayerSessions` implemented.

## Risks

- The SDK may not run in the AppContainer (step 1 exists to find out).
- Integration code could be stopped by the safety classifier, as three earlier networking pieces were.
- Epic's terms (above). Also, every player's game talks to Epic's servers, using credentials embedded in the shared build.
