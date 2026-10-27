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
  Restore-XboxLive.bat  double-click to undo every pdnet change and check Teredo (runs restore-xbl.ps1)
  pdlaunch.exe        loads pdnet into the game when it starts
  pdnet.dll           the pdnet build
  pdnet.ini           settings (name, nametags, Discord messages)
  EOSSDK-Win64-Shipping.dll   Epic Online Services runtime
  SDL3.dll            reads PlayStation, Nintendo and other controllers for the game (SDL, zlib licence)
  eos.ini             connection settings for the test. Please don't share it further.
  setup.ps1, remove.ps1, collect-log.ps1, nosuspend.ps1   the scripts behind the .bat files

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

Settings (pdnet.ini in the pdnet folder)
-----------------------------------
Change them in the game: multiplayer menu > Settings (left / right on a row; Name: press left or right, type,
Enter). Or edit pdnet.ini with Notepad, then restart the game.
  display_name=          your name (Setup.bat sets it)
  nametags_on_bodies=    on/off: defeated players keep their nametag. In a lobby the HOST's setting applies.
  nametag_colors=        on/off: coloured nametags (off: all white)
  dead_nametag_color=    colour of a defeated player's nametag (RRGGBB), FFE600 = yellow
                         (also shown on a player lying in Play Dead, until they move or are hit)
  ally_nametag_color=    teammates' nametags (Tag Team), RRGGBB
  enemy_nametag_color=   opponents' nametags, RRGGBB
  voice_chat=            on/off: voice chat in lobbies (through Epic, no account needed)
  voice_mode=            mic: the in-game option is called Mic and turns only your microphone on/off
                         classic: the original Voice Chat option - Off means you neither talk nor hear
  voice_listen=          on/off: hear other players (mic mode only)
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
