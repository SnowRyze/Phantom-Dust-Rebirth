# Phantom Dust Rebirth (pdnet)

Online play for the 2017 Windows version of Phantom Dust, now that Xbox Live matchmaking is gone. pdnet stands in for
the game's Xbox Live library and runs lobbies, matches, voice chat and scoreboards over Epic Online Services. It also
adds a few fixes and features (see [`docs/HANDOFF.md`](docs/HANDOFF.md)). The game files themselves are never changed: a small launcher loads
pdnet when the game starts.

**Download:** the newest Stable on the [Releases](https://github.com/SnowRyze/Phantom-Dust-Rebirth/releases) page:
`PhantomDust-Rebirth-Stable-<N>-PC.zip` for Windows, `PhantomDust-Rebirth-Linux.flatpak` for Linux. Unzip the kit to a
folder that stays put and double-click `Setup.bat`.

## Features

Everything here is in the current Stable (Stable 13). *(Not yet confirmed)* means it hasn't been checked in a real match
yet. Newer work goes into Experimental builds first.

**Playing online**
- Lobbies, matches, voice chat and scoreboards through Epic Online Services: no Xbox Live, no Epic account, no port
  forwarding.
- Works with the Microsoft Store version and with unpacked copies, and on Linux (wine-uwp, or the Flatpak).
- Your own player name, kept when the game is reinstalled. Saves are kept on your PC.
- Joins that survive slow or lost connection notices, PCs with wrong clocks, and short connection drops (matches pause
  instead of freezing). Epic's relay is used automatically when two PCs can't reach each other directly.
- Empty lobbies hidden; lobby join and leave crashes fixed.
- Setup lets the game through the Windows Firewall, and pdnet checks the network at start-up and logs what blocks it.
- Lobby announcements in Discord (opened, who joined, full, closed).

**Lobbies and scoreboards**
- Rated and Unrated lobbies; any rule can be Rated, and there is no rating limit for joining.
- Global scoreboards are back: Rating, Battle Royale, Tag Team and One-on-One wins.
- Arsenal Lab button in the lobby, for every player.

**Voice chat**
- Voice chat in lobbies and matches; in Tag Team matches you hear only your team.
- Mind Reading lets you hear the other team, as the skill says.
- Mic mode (the option mutes only your microphone) or Classic mode (the original Voice Chat on/off).
- A megaphone icon shows who is talking.

**Nametags**
- Always on, with your own colours for teammates, opponents and defeated players.
- Optional nametags on defeated players (the lobby host's setting applies); Play Dead shows as defeated.
- A player who leaves the match loses their nametag.
- AI players in the debug menu's battles get nametags too.

**Arsenal Lab**
- Arsenal cases follow the arsenal: 1, 2 or 3 schools, up to 3, so the case type and Aura rate always match the skills
  in it. The Limit shown follows the schools while you edit *(not yet confirmed)*.
- Buy and sell arsenal cases in the Lab, also from the lobby.
- Test Arsenal: pick your arsenal and character, the AI's arsenal and character, and the map, then fight.
- The skill preview works online, in the Arsenal editor and View Stock List, also from the lobby. Pressing Select
  there no longer freezes it *(not yet confirmed)*.
- Max Credits & Skills in the shop: up to 5,000,000 credits and 80 of every skill, DLC included.

**Menus**
- "Rebirth" title artwork and the build version on the main menu.
- Community (Discord) button in place of Help & Support; How to Play opens phantomdust.online.
- "Xbox Live" reads "Online"; the multiplayer menu has Online Settings (name, voice, nametags, Discord posts, partner
  aim), and the Online Store is gone.
- Options: Send Log (sends your logs to the developers), Updates (Stable, Experimental or Off) and Aura Particle Beeps.
- Character select shows each colour's name instead of COLOR 1-4 (Alpha's *not yet confirmed*).
- Player feedback screen after matches removed.

**Controls**
- PlayStation, Nintendo and other controllers work like an Xbox controller, with rumble.
- Keyboard works through the launcher; keys can be rebound in Options > Keyboard.
- Lock onto partner in Tag Team: RB or Tab (key configurable), Modern or Classic in Online Settings *(not yet confirmed)*.
- B skips the start-up logos and movies, and leaves a match load that has hung for 30 s *(not yet confirmed)*.

**Other**
- Updates itself at game start from this repository's releases (your settings are kept).
- Quieter start-up logos, and the game's volume is remembered.
- PD Helper modpacks stay loaded through matches *(not yet confirmed)*.
- No pausing when the game is minimized.
- One-click `Setup.bat`, `Remove.bat`, `Collect-Log.bat` and `Edit-Settings.bat`.

## Building

You need, once:

1. **Visual Studio 2022** (the free Community edition, or "Build Tools for Visual Studio 2022") with the workload
   **Desktop development with C++**: <https://visualstudio.microsoft.com/downloads/>
2. **Windows SDK for Windows 11 (10.0.22000.194)**, exactly this version. Installer:
   <https://go.microsoft.com/fwlink/?linkid=2173743>
   - Run the downloaded `winsdksetup.exe` and keep its default selection. It installs next to any newer SDK.
   - Newer SDKs don't fit the generated Xbox Live code, and current Visual Studio no longer offers this one as a
     checkbox.
   - It's listed on Microsoft's [Windows SDK archive](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads-archive)
     as "Windows SDK for Windows 11 (10.0.22000.194)", Installer.
   - If it's missing, `build.bat` offers to open the download.
3. **Epic's EOS SDK** for C, Windows: sign in at <https://dev.epicgames.com/portal>, open *SDK & Release Notes*,
   download it and unzip it into `deps\eos-sdk`. Its license doesn't allow it in this repository.

Then double-click **`build.bat`**. A few minutes later you have:

| | |
|---|---|
| `out\kit\` | the kit, ready to use: run its `Setup.bat` to install on this PC |
| `out\PhantomDust-pdnet.zip` | the same kit as a zip, to share |
| `out\pdnet.dll`, `out\pdlaunch.exe` | the mod and its launcher on their own |

For online play the kit also needs the EOS client settings and, optionally, the Discord links. Put them in
`private\` (never in git):

- `private\eos.tester.ini`: the EOS client the kit's players sign in with (copied into the kit as `eos.ini`)
- `private\links.ini`: `discord_webhook=`, `log_webhook=` and `discord_invite=` lines, copied into the kit's `pdnet.ini`

Without them `build.bat` still builds and says what is missing.

### Updates

Installed kits update themselves: at game start `pdlaunch.exe` looks at this repository's GitHub Releases and, if
there is a newer build on its channel, downloads and installs it before the game goes online. The player's `eos.ini`
and `pdnet.ini` stay as they are. The channel is `update_channel=` in the kit's `pdnet.ini`: `stable`, `experimental`
or `off`; empty means the kit's own kind. Its log is the `update:` lines in `pdnet\pdlaunch.log`.

**Publishing updates.** `build.bat experimental "notes"` publishes the kit as a GitHub pre-release (`build-<N>`), and
`build.bat promote <N>` publishes the Stable as a release (`stable-<M>`). Each carries `pdnet-update-<N>.zip`, the kit
without `eos.ini` and without the Discord links. It needs, once, a GitHub token in `private\github-token.txt`:

1. github.com > your picture > Settings > Developer settings > Personal access tokens > Fine-grained tokens >
   Generate new token.
2. Repository access: *Only select repositories*, this repository. Permissions: **Contents: Read and write**.
3. Copy the token into `private\github-token.txt` (that folder is never in git).

Commit and push before `build.bat experimental`: a release has to point at a commit that is on GitHub.

### Linux

`build-linux.sh` builds pdnet.dll on Linux with clang-cl and lld-link, for running the game under wine-uwp. Its header
lists what it needs (xwin's Windows SDK, a C++/WinRT projection, Epic's SDK). Contributed by DeadMan009.

`flatpak/` packages wine-uwp, a Stable kit and the kit's Linux setup as a Flatpak, without any game or Windows files
(see [flatpak/README.md](flatpak/README.md)). The **Flatpak** workflow builds it for each Stable release.

### Numbered kits

- `build.bat experimental "what changed"` also saves the kit as the next `PhantomDust-pdnet Experimental <N>.zip`
  (on `D:\` if there is one, else in `out\kits`; set `PDNET_KITS` to choose).
- `build.bat promote <N>` makes Experimental N the next `Stable <M>` (the public version, with the links from
  `private\public-links.ini`), tags its source commit `stable-<M>` and publishes it with the whole kit
  (`PhantomDust-Rebirth-Stable-<M>-PC.zip`) and the update zip.
- `build.bat conceptual "what it tries"` (on the `Conceptual` branch) saves the next `PhantomDust-pdnet Conceptual <N>.zip`.
  Conceptual kits are for very experimental work: they are never published, and they never update themselves. Changes go from master into `Conceptual` only, never back.

## What's where

| Folder | |
|---|---|
| `src\` | pdnet itself (C++): EOS lobbies, P2P, voice, scoreboards, game patches |
| `xsapi\` | the stand-in for the game's Xbox Live library (`gen\` is generated by `xsapi\generate.py`, committed so nobody needs to run it) |
| `launcher\` | `pdlaunch.exe`, which loads pdnet into the game and handles volume, controllers and the clock check |
| `kit\` | everything in a kit besides the built files: `Setup.bat`, the scripts, `README.txt`, `pdnet.ini` |
| `deps\` | SDL3 (controllers, in the repository) and Epic's SDK (you add it) |
| `build-linux.sh` | the Linux build (see above) |
| `tools\` | `build.ps1` (what `build.bat` runs), `kit-counter.txt` (the last Experimental number), `rebirth-word\` (the title lettering) |
| `docs\` | `HANDOFF.md`, the full development log (how everything works and why), and the early planning notes |
