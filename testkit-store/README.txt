PHANTOM DUST (2017 PC, MICROSOFT STORE) - pdnet STAGE C TEST BUILD (no Xbox Live, no port forwarding)
=====================================================================================================

What this is
------------
The same pdnet stage C test build as the unpacked-copy kit, for the normal Microsoft Store install of
Phantom Dust. The game's files are NOT changed: when the game starts, Windows runs pdlaunch.exe from this
folder, which loads pdnet into the game.
- No Xbox sign-in. Your player name comes from this kit (you choose it during setup).
- Lobbies and matches go through Epic Online Services (EOS). No Epic account needed, no port forwarding.
- Saves are stored locally while pdnet is set up (your Xbox cloud progress will NOT show up; it is
  untouched and comes back after Remove.bat).
- Both players must use stage C (this kit or the unpacked-copy kit with the same build).

This is the first test of the Store version, so it may not work at all yet. Every result helps.

What's in the kit
-----------------
  README.txt          this guide
  Setup.bat           double-click to set up pdnet (runs store-setup.ps1)
  Remove.bat          double-click to go back to the normal game with Xbox Live (runs store-remove.ps1)
  Collect-Log.bat     double-click after a test: copies the logs to your Desktop (runs collect-log.ps1)
  pdlaunch.exe        loads pdnet into the game when it starts
  pdnet.dll           the pdnet build
  pdnet.ini           settings (name, nametags, Discord messages)
  EOSSDK-Win64-Shipping.dll   Epic Online Services runtime
  eos.ini             connection settings for the test. Please don't share it further.

Steps
-----
1. Install Phantom Dust from the Microsoft Store if you haven't, and start it once normally. Then close it.
2. Unzip this kit to a folder that will stay put, e.g. Documents\PhantomDust-pdnet.
   (Not inside the zip preview, and don't move or delete the folder while pdnet is set up.)
3. Double-click Setup.bat.
   - Type your player name when asked.
   - Windows asks for administrator permission once ("CheckNetIsolation"). Click Yes: it lets the game
     use connections on this PC, which matches need.
4. Start Phantom Dust the normal way (Start menu). Host or join a lobby.
5. After the test, double-click Collect-Log.bat and send the files it puts on your Desktop.

To play normally again (Xbox Live), double-click Remove.bat.

Settings (pdnet.ini in this folder)
-----------------------------------
Edit with Notepad, then restart the game.
  display_name=          your name (Setup.bat sets it)
  nametags_on_bodies=    on/off: defeated players keep their nametag. In a lobby the HOST's setting applies.
  dead_nametag_color=    colour of a defeated player's nametag (RRGGBB), FFE600 = yellow
                         (also shown on a player lying in Play Dead, until they move or are hit)
  lobby_arsenal_editor=  on/off: the lobby's Invite button opens the Arsenal editor
  discord_*              lobby announcements in Discord when you host

If something goes wrong
-----------------------
- The game starts with Xbox Live as usual: pdnet didn't load. Run Collect-Log.bat and send pdlaunch-log.
- "Windows refused to attach pdlaunch": send a screenshot.
- Lobbies work but matches don't connect: run Setup.bat again and allow the administrator prompt.
