PHANTOM DUST (2017 PC) - pdnet TEST BUILD (no Xbox Live, no port forwarding)
============================================================================

What this is
------------
pdnet replaces Xbox Live in Phantom Dust. It works with the normal Microsoft Store install AND with an
unpacked copy, and it does NOT change any of the game's files: when the game starts, Windows runs pdlaunch.exe
from this folder, which loads pdnet into the game.
- No Xbox sign-in. Your player name comes from this kit (you choose it during setup).
- Lobbies and matches go through Epic Online Services (EOS). No Epic account needed, no port forwarding.
- Saves are stored locally while pdnet is set up. On the first start pdnet copies your Xbox cloud save
  (story progress, collection, arsenals, play time) into them, if you're signed in to Xbox and have no pdnet
  saves yet. The cloud save itself is never changed, and later progress stays local.
- Everyone you play with needs this kit (or a newer one).

What's in the kit
-----------------
  README.txt          this guide
  Setup.bat           double-click to set up pdnet (runs setup.ps1)
  Remove.bat          double-click to stop using pdnet (runs remove.ps1)
  Collect-Log.bat     double-click after a test: copies the logs to your Desktop (runs collect-log.ps1)
  Restore-XboxLive.bat  double-click to undo every pdnet change and check Teredo (runs restore-xbl.ps1)
  pdlaunch.exe        loads pdnet into the game when it starts
  pdnet.dll           the pdnet build
  pdnet.ini           settings (name, nametags, Discord messages)
  EOSSDK-Win64-Shipping.dll   Epic Online Services runtime
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
   - Windows asks for administrator permission once ("CheckNetIsolation"). Click Yes: matches need it.
4. Start Phantom Dust the normal way (Start menu). Host or join a lobby.
5. After a test, double-click Collect-Log.bat and send the files it puts on your Desktop.

To stop using pdnet, double-click Remove.bat.
To go fully back to Xbox Live and Teredo (also undoes older kits that replaced game files), double-click
Restore-XboxLive.bat. It asks for administrator permission once.

Settings (pdnet.ini in this folder)
-----------------------------------
Edit with Notepad, then restart the game.
  display_name=          your name (Setup.bat sets it)
  nametags_on_bodies=    on/off: defeated players keep their nametag. In a lobby the HOST's setting applies.
  dead_nametag_color=    colour of a defeated player's nametag (RRGGBB), FFE600 = yellow
                         (also shown on a player lying in Play Dead, until they move or are hit)
  lobby_arsenal_editor=  on/off: the lobby's Arsenal Lab button (was Invite) opens the Arsenal editor
  cloud_save_import=     on/off: copy the Xbox cloud save on the first start (once)
  discord_*              lobby announcements in Discord when you host

If something goes wrong
-----------------------
- The game starts with Xbox Live as usual: pdnet didn't load. Run Collect-Log.bat and send pdlaunch-log.
- "Windows refused to attach pdlaunch": send a screenshot.
- Lobbies work but matches don't connect: run Setup.bat again and allow the administrator prompt.
