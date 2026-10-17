# Phantom Dust (2017 UWP) — Replacing Xbox Live / Teredo networking

Dump: `D:\PhantomDust-Dumped-NoMovie` — package `Microsoft.MSEsper` v1.3.25854.2, x64, TitleId 267248679,
SCID `164d0100-a5f0-44b8-90ce-bf3a0fede427`. Files are decrypted (valid PE), no AppxSignature/BlockMap,
so the folder can be registered directly in Developer Mode.

## 1. What the recon found

| Binary | Role | Xbox dependency |
|---|---|---|
| `PDUWP.exe` (C++/CX) | App shell, multiplayer + networking glue | Heavy (see below) |
| `WPFLibRelease_UWP.dll` | Game engine (original Phantom Dust code) | None |
| `D3D11LibRelease_UWP.dll` | Renderer | None |
| `MeldRelease_UWP.dll` | Data/UI layer, JSON loader, title storage (`MeldTitleStorage.cpp`) | TitleStorage, GetTokenAndSignature, `showmarketplaceasync` |
| `Microsoft.Xbox.Services.dll` + `.winmd` | XSAPI (WinRT flavour), registered in `AppxManifest.xml` as an in‑process server | **Is** Xbox Live — replaceable |
| `Microsoft.Xbox.GameChat.dll`, `Microsoft.Xbox.ChatAudio.dll` | Voice chat | Replaceable / stub |
| `cpprest140_uwp_2_9.dll` | HTTP for XSAPI | — |

The good news: **the game engine itself doesn't touch Xbox Live at all.** Everything is in `PDUWP.exe`'s
glue code, built from Microsoft's public Xbox multiplayer samples.

### The online stack inside PDUWP.exe

1. **Identity:** `Microsoft.Xbox.Services.System.XboxLiveUser` + `Windows.System.User`. Peers are
   identified by XUID (`PDUWP.MPUtilities.MultiplayerActivationInfo`: Sender/Target/Joinee XUID, HandleId).
2. **Sessions and matchmaking:** `PDUWP.Xbox.Samples.Multiplayer.MultiplayerManager`, a task queue
   (`CreateSessionFromTemplateTask`, `CreateSessionFromHandleTask`, `BeginSmartMatchTask`,
   `CancelMatchmakingTask`, `AddReservationsTask`, `BecomeHostTask`, `SimpleHostMigrationTask`,
   `LeaveSessionTask`, `SetSessionJoinRestrictionTask`, `UpdateSearchHandleTask`, `RefreshSessionsTask`,
   `WriteSessionPropertiesTask`, `SetTransferHandleTask`, `SetActivityTask` …). It runs on XSAPI's
   MPSD sessions, SmartMatch (`CreateMatchTicket`, "Hopper"), search handles (the lobby browser) and
   RTA session-change subscriptions.
3. **Transport:** `PDUWP.NetworkMesh.MeshManager`. **Plain Winsock UDP** (`WSASocketW`, `WSASendTo`,
   `WSARecvFrom`) on port **43210** (`networkmanifest.xml` → `PeerSocketUDP`). The only Xbox part is
   *addressing*: peers swap `XboxLiveDeviceAddress` snapshots, then `XboxLiveEndpointPair`
   (Secure Device Association, template `PeerToHostUDP`) gives them the Teredo/IPsec socket address.
   Received packets are mapped back to a peer through the endpoint pair (`SecureDeviceAddress::FromBytes`).
   Message types: `GAME_HELLO_DATA`, `GAME_HEARTBEAT_DATA`, `GAME_ACK`, `GAME_CHAT_DATA`, `GAME_CUSTOM_DATA`.
4. **Online data:** XSAPI TitleStorage (card definitions and balance, per the devs). There are local copies in
   `Assets\Data\gstorage\gsdata_*.dat`, and `JsonLoader::SetLoadFileFromServerFunc` /
   `m_hackFunction` in Meld is a ready-made hook point.
5. **Saves:** `Windows.Gaming.XboxLive.Storage.GameSaveProvider` (Connected Storage). **Saves break
   without an Xbox sign-in unless we replace this.**
6. **Extras:** Leaderboards, UserStatistics, Social profiles, Presence, GameChat voice, TitleCallableUI,
   marketplace UI.

**So Teredo is only used to get a peer's address. The game packets are ordinary UDP.**
Swap the addressing layer and the session/matchmaking layer, and the game's own mesh code carries on as before.

## 2. Architecture of the replacement

```
 PDUWP.exe (unchanged, or minimally patched)
   │  activates WinRT classes by name
   ▼
 pdnet.dll  ── shipped AS Microsoft.Xbox.Services.dll (same activatable class IDs)
   ├─ XSAPI surface: only the classes/methods the game uses; everything else → E_NOTIMPL
   │    XboxLiveUser / XboxLiveContext   → local profile (name + stable fake XUID + token)
   │    MultiplayerService / Matchmaking → our lobby server (HTTPS/WebSocket)
   │    RealTimeActivity                 → server push (WebSocket)
   │    TitleStorage                     → server blob, or the local gstorage files
   │    Leaderboard / Stats / Social     → server, or empty results
   ├─ Hook on RoGetActivationFactory (we're loaded in-process early) to take over OS classes:
   │    Windows.Networking.XboxLive.*    → fake SDA: device address = {peer id, ip:port};
   │                                       endpoint pair = plain sockaddr (direct or via relay)
   │    Windows.Gaming.XboxLive.Storage.*→ files under LocalState
   │    Windows.System.User (if needed)
   └─ GameChat/ChatAudio → stubbed at first

 pdserver (one small service)
   ├─ accounts (username + token, no Microsoft account)
   ├─ sessions / lobbies / search handles (MPSD-shaped JSON, only the fields the game reads)
   ├─ matchmaking queue (SmartMatch replacement)
   ├─ rendezvous: tells each peer the other's public ip:port; UDP hole punching
   ├─ UDP relay fallback when hole punching fails
   └─ title storage: serves the card data blob(s)
```

Tooling: generate the WinRT component skeleton straight from the original metadata with
`cppwinrt.exe -in Microsoft.Xbox.Services.winmd -component`, so the ABI is exact. The same goes for the
`Windows.Networking.XboxLive` / `Windows.Gaming.XboxLive.Storage` classes from the Windows SDK metadata.

**Rejected alternative:** detouring the game's own `MultiplayerManager` tasks inside `PDUWP.exe`. It's a smaller
surface, but it's tied to code addresses and needs far more Ghidra work. Keep it as plan B for any call that's awkward to fake at the WinRT level.

## 3. Packaging changes (dev-mode registration)

- Copy the dump to a working folder. Never edit the pristine dump.
- `AppxManifest.xml`:
  - Change `Identity Name` (e.g. `PhantomDust.Community`) so it can't clash with the Store version.
  - Add the capabilities `internetClientServer` and `privateNetworkClientServer`. **Without these the
    AppContainer blocks inbound UDP**, because the original relied on SDA/IPsec, which is exempt.
  - Leave the `inProcessServer` entries alone. Our DLL takes the same filename.
- `Add-AppxPackage -Register .\AppxManifest.xml` (Developer Mode on).
- **Testing on one PC:** register a *second* copy under a different Identity Name so two instances can run at once,
  and exempt both from loopback isolation (`CheckNetIsolation LoopbackExempt -a -n=<PFN>`).

## 4. Milestones

| # | Goal | Done when |
|---|---|---|
| M0 | Dump boots from dev-mode registration | Title screen + single player work; we know what breaks with no Xbox sign-in |
| M1 | **Spy shim:** a pass-through `Microsoft.Xbox.Services.dll` that loads the renamed original, forwards everything and logs every class activation; also logs `Windows.*XboxLive*` activations | Exact list of the classes/methods the game uses → the scope of everything after this |
| M2 | Ghidra pass on `MeshManager` + `MultiplayerManager` | Socket family/flags known; how the remote sockaddr is taken from the endpoint pair; which session properties carry the device address and game state |
| M3 | Offline identity + local saves | Boots with no Microsoft account, local profile name shown, saves persist |
| M4 | **Direct-IP match** — fake SDA, host/join via a config file (`peer=1.2.3.4:43210`) | Two clients finish a match with no Xbox Live and no Teredo |
| M5 | `pdserver` lobbies + matchmaking replacing MPSD/SmartMatch/search handles | Quick Match and the lobby list work against our server; host migration works |
| M6 | NAT traversal: hole punching + relay fallback | Matches work between two home connections without port forwarding |
| M7 | Title storage / card data from server; stats/leaderboards; optional voice chat | Feature parity |

## 4a. Status (2026-10-17)

- The dump is **already registered** from `D:\PhantomDust-Dumped-NoMovie` as `Microsoft.MSEsper_8wekyb3d8bbwe`
  (Developer Mode, unsigned, Status OK). Its LocalState is empty, so it has probably never been run from here.
- Visual Studio 2022 Community is installed, **but without the C++ desktop or UWP C++ workloads**.
  Windows SDK 10.0.22000 (with `cppwinrt.exe`) is present.
- Test setup: **2 PCs**. Server language: **Python** for now (easy to read and change; can be ported later).

### Progress log

- **2026-10-17: M0 done, M3 (saves) partly done.** The dump boots with the real Xbox sign-in, but hung on
  "retrieving profile" because `GameSaveProvider.GetForUserAsync` (Xbox Connected Storage) never completes for the
  sideloaded copy. `pdnet` stage 1 (`_netmod\pdnet`) now answers that class with local files under
  `LocalState\pdnet\saves`. The game gets past the hang to profile creation and writes `UserConfig/tms_options.csv` and
  `UserConfig/gsdata_en.dat`.
  Build: `_netmod\pdnet\build.cmd` · install/undo: `install.ps1` / `uninstall.ps1` (original backed up in `_netmod\original`).
  Log: `%LOCALAPPDATA%\Packages\Microsoft.MSEsper_8wekyb3d8bbwe\LocalState\pdnet.log`.

- **2026-10-17: stage 2 code done (direct UDP instead of Teredo), and the builds are split.**
  `pdnet` now also replaces `Windows.Networking.XboxLive.*`. Device addresses are 34-byte pdnet snapshots; endpoint
  pairs come from a CONNECT/ACK handshake on UDP 43211; QoS uses real pings. The game socket (dual-stack UDP 43210,
  confirmed in Ghidra: `FUN_140027870` creates it, `FUN_140029080` sends through `GetRemoteSocketAddressBytes`) is unchanged.
  - `D:\PhantomDust-Direct`: `network_mode=direct`; the manifest adds `internetClientServer` + `privateNetworkClientServer`.
  - `D:\PhantomDust-Teredo`: `network_mode=teredo` (original Microsoft networking; only local saves replaced).
  - `D:\PhantomDust-Dumped-NoMovie`: master, restored byte-for-byte to the dump as received (verified against `_netmod\original`).
  - Same package identity for all three (Xbox Live auth), so one is registered at a time: `_netmod\builds.ps1 -Use Direct|Teredo|Master`.
    Saves are shared and preserved across switches. Save backup: `_netmod\backup\LocalState-2026-10-17`.

## 4b. Xbox surface the game actually uses (static scan)

These are the classes that appear in RTTI/activation strings of the game binaries. They're the full scope of the
replacement DLL plus the OS-class shims. **`WPFLibRelease_UWP.dll` (the engine) references none of them.**
`MeldRelease_UWP.dll` uses only TitleStorage, XboxLiveContext, TitleCallableUI and `GetTokenAndSignature`.
That last one suggests Meld calls a separate web backend with an Xbox token, which needs checking.

| Namespace | Classes |
|---|---|
| `Microsoft.Xbox.Services` | XboxLiveContext, XboxServiceCallRoutedEventArgs |
| `…System` | XboxLiveUser, SignOutCompletedEventArgs, TitleCallableUI, GetTokenAndSignatureResult |
| `…Multiplayer` | MultiplayerSession, MultiplayerSessionReference, MultiplayerSessionMember, MultiplayerSessionChangeEventArgs, MultiplayerSubscriptionLostEventArgs, WriteSessionResult, MultiplayerSearchHandleRequest, MultiplayerQuerySearchHandleRequest, MultiplayerQualityOfServiceMeasurements (+ enums MatchmakingStatus, MemberStatus, Restriction, Visibility, WriteSessionStatus) |
| `…Matchmaking` | CreateMatchTicketResponse |
| `…TitleStorage` | TitleStorageBlobMetadata, TitleStorageBlobResult |
| `…Leaderboard` / `…UserStatistics` | LeaderboardResult / UserStatisticsResult |
| `…Social` / `…Presence` / `…RealTimeActivity` | XboxUserProfile, SocialGroupConstants / PresenceData / subscription state enum |
| `Microsoft.Xbox.GameChat` | ChatManager (+ event/handler types) |
| `Windows.Networking.XboxLive` | XboxLiveDeviceAddress, XboxLiveEndpointPair(+State, CreationResult, StateChangedEventArgs), XboxLiveEndpointPairTemplate, XboxLiveInboundEndpointPairCreatedEventArgs, XboxLiveQualityOfServiceMeasurement |
| `Windows.Gaming.XboxLive.Storage` | GameSaveProvider, GameSaveProviderGetResult, GameSaveBlobGetResult, GameSaveOperationResult |
| `Windows.System` | User, UserChangedEventArgs, UserPicker |

No Inventory, Marketplace or Achievements classes are referenced.

## 5. Open questions / risks

- **Socket family:** Teredo addresses are IPv6. If the mesh opens an IPv6-only socket, the fake endpoint pair has to
  hand back IPv4-mapped addresses (`::ffff:a.b.c.d`), or we hook `WSASocketW` to clear `IPV6_V6ONLY`. M2 answers this.
- **License/Store checks** on a dev-registered dump — confirm in M0.
- **How much state lives in MPSD session properties.** Lobby/game settings may be stored as custom session JSON,
  and the server has to round-trip that verbatim.
- **Card ownership / currency:** find out whether it lives in the saves, title storage or a service. The project's
  goal is restoring play, not unlocking paid content.
- **P2P trust:** host-authoritative peer-to-peer means a modified client can cheat. That's acceptable for community play,
  but note it.
