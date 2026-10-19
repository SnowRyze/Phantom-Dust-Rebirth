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

## Risks

- The SDK may not run in the AppContainer (step 1 exists to find out).
- Integration code could be stopped by the safety classifier, as three earlier networking pieces were.
- Epic's terms (above). Also, every player's game talks to Epic's servers, using credentials embedded in the shared build.
