PHANTOM DUST (2017 PC) - pdnet TEST BUILD (no Xbox Live, no port forwarding)
============================================================================

What this is
------------
pdnet replaces Xbox Live in Phantom Dust. It works with the normal Microsoft Store install AND with an
unpacked copy, and it does NOT change any of the game's files: when the game starts, Windows runs pdlaunch.exe
from the kit's pdnet folder, which loads pdnet into the game.
- No Xbox sign-in. Your player name comes from this kit (you choose it during setup).
- Lobbies and matches go through Epic Online Services (EOS). No Epic account needed, no port forwarding.
- Saves are stored locally while pdnet is set up. To bring your Xbox progress over (story, collection,
  arsenals, play time), use IMPORT XBOX SAVE in the game's menu (it was Help & Support), then restart the game.
  Your previous pdnet saves are kept in a backup folder, and the Xbox cloud save itself is never changed.
  This needs the Microsoft Store version signed in to Xbox.
- Everyone you play with needs this kit (or a newer one).
- Your player (what the scoreboards count) is kept in Windows Credential Manager ("pdnet/identity/..."), so
  reinstalling the game or deleting the kit doesn't turn you into a new player. Don't delete those entries.

What's in the kit
-----------------
  Setup.bat           double-click to set up pdnet (runs pdnet\setup.ps1)
  pdnet\              everything else:
  README.txt          this guide
  Remove.bat          double-click to stop using pdnet (runs remove.ps1)
  Collect-Log.bat     double-click after a test: copies the logs to your Desktop (runs collect-log.ps1)
  Edit-Settings.bat   double-click to open the in-game Settings file (settings.ini) in Notepad
  Restore-XboxLive.bat  double-click to undo every pdnet change and check Teredo (runs restore-xbl.ps1)
  pdlaunch.exe        loads pdnet into the game when it starts
  pdnet.dll           the pdnet build
  pdnet.ini           settings (name, nametags, Discord messages)
  EOSSDK-Win64-Shipping.dll   Epic Online Services runtime
  SDL3.dll            reads PlayStation, Nintendo and other controllers for the game (SDL, zlib licence)
  eos.ini             connection settings for the test. Please don't share it further.
  setup.ps1, remove.ps1, collect-log.ps1, nosuspend.ps1   the scripts behind the .bat files
  setup-linux.sh      Linux (wine-uwp) setup, see below

Steps
-----
1. Microsoft Store version: install Phantom Dust from the Store if you haven't, start it once normally, close it.
   Unpacked copy: Developer Mode must be on (Settings > (Update & Security >) For developers).
2. Unzip this kit to a folder that will stay put, e.g. Documents\PhantomDust-pdnet.
   (Not inside the zip preview, and don't move or delete the folder while pdnet is set up.)
3. Double-click Setup.bat.
   - Unpacked copy that isn't installed yet: it asks for the game folder and installs it.
   - Unpacked copy set up with an OLDER kit (Install.bat): it puts the original game files back first.
   - Type your player name when asked.
   - Answer Y to let Phantom Dust use your microphone (voice chat). N: Windows asks the first time instead.
   - Windows asks for administrator permission once ("CheckNetIsolation"). Click Yes: matches need it.
4. Start Phantom Dust the normal way (Start menu). Host or join a lobby.
5. After a test, double-click pdnet\Collect-Log.bat and send the files it puts on your Desktop.

To stop using pdnet, double-click pdnet\Remove.bat.
To go fully back to Xbox Live and Teredo (also undoes older kits that replaced game files), double-click
pdnet\Restore-XboxLive.bat. It asks for administrator permission once.

Linux (wine-uwp)
----------------
Runs under wine-uwp (https://github.com/LiveManQuestion/wine-uwp), a Wine build for Microsoft Store games.
Needs a Microsoft account that owns Phantom Dust (free in the Store: "Get" it once at
https://apps.microsoft.com/detail/9PCDNBHR11MR), about 25 GB free, a recent 64-bit distro (glibc 2.38+: Ubuntu 24.04,
Debian 13, Fedora 39, Mint 22, Arch or newer), and GStreamer for the game's videos:
  Ubuntu/Debian/Mint: sudo apt install curl gstreamer1.0-plugins-good gstreamer1.0-plugins-base gstreamer1.0-libav
  Fedora:             sudo dnf install curl gstreamer1-plugins-good gstreamer1-plugins-base gstreamer1-plugin-libav
  Arch:               sudo pacman -S curl gst-plugins-good gst-plugins-base gst-libav
1. Unzip the kit, open a terminal in its pdnet folder and run:  bash setup-linux.sh
   It downloads wine-uwp (about 50 MB, to ~/.local/opt/wine-uwp) if you don't have it. If Phantom Dust isn't
   installed yet it installs it: it asks for a Windows 11 ISO (or downloads Microsoft's free one, about 7 GB), then
   shows a sign-in link for your Microsoft account and downloads the game (about 10 GB). A licence error about the
   newest version is expected; it then takes the older one, which is the one pdnet is made for.
   Then type your player name.
2. Start the game with the command it prints (~/.local/opt/wine-uwp/bin/wine-uwp run msesper).
3. After a test: bash setup-linux.sh --logs, and send the files it copies to your home folder.
Updates: as on Windows, the kit updates itself when the game starts, so keep the kit folder where it is. A wine-uwp
from before that (setup-linux.sh says so) needs  bash setup-linux.sh --update  now and then.
Either way it gets the newest build on your channel from GitHub and keeps your eos.ini and pdnet.ini. Options >
Updates in the game turns it off. If it can't update, the game starts as it is
(log: ~/.local/share/wine-uwp/logs/msesper-before-start.log).
To stop using pdnet: bash setup-linux.sh --remove. Setting up a newer kit: run its setup-linux.sh.
(On Linux the game reads pdnet.ini from its own folder, ~/.local/share/wine-uwp/stage/msesper.)

Settings (pdnet.ini in the pdnet folder)
-----------------------------------
Change them in the game: multiplayer menu > Settings (left / right on a row; Name: press left or right, type,
Enter). Or edit pdnet.ini with Notepad, then restart the game.
What you save in the game goes to settings.ini (in the game's own data folder) and overrides pdnet.ini.
To change those by hand, e.g. an exact nametag colour, close the game and double-click pdnet\Edit-Settings.bat.
Colours take any hex code: RRGGBB or #RRGGBB.
Updates: Main menu > Options > Updates picks Stable or Experimental builds, or Off for no automatic updates
(used from the next game start). Or set update_channel in settings.ini: stable, experimental or off.
  display_name=          your name (Setup.bat sets it)
  nametags_on_bodies=    on/off: defeated players keep their nametag. In a lobby the HOST's setting applies.
  dead_nametag_color=    colour of a defeated player's nametag (RRGGBB), FFE600 = yellow
                         (also shown on a player lying in Play Dead, until they move or are hit)
  ally_nametag_color=    teammates' nametags (Tag Team), RRGGBB
  enemy_nametag_color=   opponents' nametags, RRGGBB
  voice_chat=            on/off: voice chat in lobbies (through Epic, no account needed)
  voice_mode=            mic: the in-game option is called Mic and turns only your microphone on/off
                         classic: the original Voice Chat option - Off means you neither talk nor hear
  voice_listen=          on/off: hear other players (mic mode only)
  partner_aim=           what the partner lock input does in a match (Multiplayer > Settings > Partner Aim):
                         modern: locks onto your partner; classic: turns the original Lock Onto Partner option on/off,
                         so switch target can also reach your partner
  partner_lock_key=      keyboard key for that input in a match (default Tab; none = off).
                         In the game: Main menu > Options > Keyboard > Lock Onto Partner.
                         On a controller it is RB.
  splash_volume=         % of your volume from the start-up logos until you are past the title screen
  discord_*              lobby announcements in Discord when you host
  more_controllers=      on/off: PlayStation, Nintendo and other controllers work like an Xbox controller
                         (Xbox and XInput controllers always work; if a pad reacts twice, close DS4Windows / Steam)
Always on: the lobby's Arsenal Lab button, Gamertag when alone, Max Credits & Skills in the shop
(up to 5,000,000 credits, 80 of every skill incl. DLC, one 2-School Arsenal Case), the global scoreboards,
and Rated / Unrated lobbies (pick it with the Rating option when you create a game).

If something goes wrong
-----------------------
- The game starts with Xbox Live as usual: pdnet didn't load. Run pdnet\Collect-Log.bat and send pdlaunch-log.
- "Windows refused to attach pdlaunch": send a screenshot.
- Lobbies work but matches don't connect: run Setup.bat again and allow the administrator prompt.
- Nobody hears you: check the lobby's Options > Mic (Voice Chat in classic mode) is On, and that Windows allows
  Phantom Dust to use the microphone (Settings > Privacy > Microphone).
