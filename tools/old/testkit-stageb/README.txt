PHANTOM DUST (2017 PC) - pdnet STAGE B TEST BUILD (lobbies without Xbox Live)
=============================================================================

What this is
------------
An experimental build that replaces Xbox Live in Phantom Dust:
- No Xbox sign-in. Your player name comes from this kit (you choose it during install).
- Lobbies ("create match" / finding a match) go through Epic Online Services (EOS).
  You do NOT need an Epic account; your PC signs in anonymously by itself.
- During a match, players connect to each other directly over UDP (pdnet's own connection).
- Saves are stored locally (your Xbox cloud progress will NOT show up).

This is the first test with two real PCs. Things are expected to break; the point is to find
out where. Every result helps, including "it didn't work at step 2".

This kit does NOT include the game. You need your own copy.

What's in the kit
-----------------
  README.txt                   this guide
  install.ps1 / uninstall.ps1  put pdnet into your game folder / take it out again
  collect-log.ps1              copies the log to your Desktop after a test
  nosuspend.ps1                keeps the game running when minimized (install.ps1 offers it)
  pdnet.ini                    settings (copied into your game folder)
  pdnet\Microsoft.Xbox.Services.dll   the pdnet build
  EOSSDK-Win64-Shipping.dll    Epic Online Services runtime
  eos.ini                      connection settings for the test (may arrive as a separate file;
                               put it next to install.ps1). Please don't share it further.

Requirements
------------
- Windows 10 or 11, 64-bit.
- Your OWN unpacked copy of Phantom Dust (the folder containing PDUWP.exe and AppxManifest.xml),
  version 1.3.25854.2. The Microsoft Store install can't be modified (it is encrypted).
- Developer Mode ON:  Settings > (Update & Security >) For developers > Developer Mode.
- Internet access (for the lobby service).
- For the first test: both PCs on the SAME home network (see "Network" below).


STEP 1 - Install (each tester, once)
------------------------------------
1. Close Phantom Dust.
2. Unzip the whole kit into a folder. Make sure eos.ini is in that folder too.
3. Right-click install.ps1 > "Run with PowerShell".
   - Enter the path to your game folder.
   - Enter a player name. Use a DIFFERENT name from the other tester, so you can tell each other apart.
   - Answer "y" to "Keep it running in the background?".
   (Or from a command line:
    powershell -ExecutionPolicy Bypass -File install.ps1 -GameFolder "D:\Games\PhantomDust" -Name "Alice")
   It backs up everything it changes into <game folder>\_pdnet_backup.
   If you installed an earlier pdnet test kit, that's fine: this one replaces it.
4. Start "Phantom Dust (pdnet test)" from the Start menu.
5. Check: the game reaches the main menu WITHOUT asking you to sign in to Xbox, and your profile loads.
   If it hangs or crashes here, stop and go to STEP 6 (send the log).


STEP 2 - Host creates a match (tester A)
----------------------------------------
1. Go to Multiplayer and create / host a match (the same menu you would use for Xbox Live play).
2. You should get to the lobby screen with yourself in it.
3. Note the time (the clock is fine) and leave the lobby screen open.


STEP 3 - Second player finds the match (tester B)
-------------------------------------------------
1. Wait about 15 seconds after the host's lobby is up (new lobbies take a few seconds to become findable).
2. Go to Multiplayer and use the option that searches for / lists matches.
3. Write down what you see:
   - Is the host's match in the list? Under what name? How many players does it show (e.g. 1/4)?
   - If the list is empty, try searching again once or twice, 10 seconds apart.


STEP 4 - Second player joins (tester B)
---------------------------------------
1. Pick the host's match and join it.
2. BOTH testers write down:
   - Does the joining player reach the lobby screen?
   - On the HOST's screen: does the second player appear? With the right name?
   - On the JOINER's screen: do both players appear?
   - Roughly how many seconds it took.


STEP 5 - Match, leave, rejoin
-----------------------------
1. Host: start the match. Play a little (a minute is enough). Write down whether you see and can
   hit each other, and whether it lags.
2. After the match (or if it fails to start), note where you end up (lobby? menu? error message?).
3. Joiner: leave the lobby. Host: does the joiner disappear from your lobby screen?
4. Joiner: find and join the host's match again (STEP 3 and 4). Does it work a second time?
5. Optional: leave the host's lobby open for 30 minutes with the joiner in it, then try starting a match.


STEP 6 - Send the results (each tester)
---------------------------------------
1. Close the game.
2. Right-click collect-log.ps1 > "Run with PowerShell". It saves pdnet-log-<PC name>-<date>.txt
   on your Desktop and shows a short summary.
3. Send that file, plus your notes from steps 2-5 and:
   - same home network or internet?
   - the player names you both used
   - anything odd (error messages, freezes, wrong names), with the time it happened.
   Screenshots of the lobby screens are very welcome.


Network
-------
- SAME home network (do this first): nothing to set up.
- Over the INTERNET: finding and joining the lobby should work anyway (it goes through Epic's
  service). The match itself only connects if the HOST forwards UDP ports 43210 and 43211 on their
  router to their PC and sets  advertise_ip=<host's public IP>  in pdnet.ini (in the game folder).
  The joiner doesn't change anything. Automatic connection without port forwarding is not in this build.


If something goes wrong
-----------------------
- "eos.ini is missing": put the eos.ini you were sent next to install.ps1 and run it again.
- "Developer Mode is off" / "Missing Microsoft.VCLibs": follow the message, then run install.ps1 again.
- The game drops you from the lobby when you alt-tab: run nosuspend.ps1 once (right-click > Run with
  PowerShell), then restart the game.
- Want your normal setup back: close the game and run uninstall.ps1. Original files are restored
  from _pdnet_backup. Local saves are kept.


Settings (pdnet.ini in the game folder; restart the game after editing)
-----------------------------------------------------------------------
display_name=...      your name as others see it (max 32 characters)
xbox=off              pdnet replaces Xbox Live
eos=on                lobbies through Epic Online Services
network_mode=direct   pdnet's direct UDP connections during matches
advertise_ip=auto     or the host's public IP for internet play with port forwarding
game_port=43210       must stay 43210 (matches the game)
control_port=43211    pdnet handshake/ping port
test_peer=off         debug only; leave off
