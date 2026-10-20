PHANTOM DUST (2017 PC) - pdnet STAGE C TEST BUILD (no Xbox Live, no port forwarding)
====================================================================================

What this is
------------
An experimental build that replaces Xbox Live in Phantom Dust:
- No Xbox sign-in. Your player name comes from this kit (you choose it during install).
- Lobbies and matches both go through Epic Online Services (EOS). You do NOT need an Epic account;
  your PC signs in anonymously by itself.
- NEW in stage C: match traffic goes through Epic's P2P service. It connects the players through their
  home routers by itself, and falls back to Epic's relay servers when it can't. Nobody has to forward
  ports or change router settings.
- Saves are stored locally (your Xbox cloud progress will NOT show up).

Things are expected to break; the point is to find out where. Every result helps.

This kit does NOT include the game. You need your own unpacked copy.

What's in the kit
-----------------
  README.txt                   this guide
  install.ps1 / uninstall.ps1  put pdnet into your game folder / take it out again
  collect-log.ps1              copies the log to your Desktop after a test
  nosuspend.ps1                keeps the game running when minimized (install.ps1 offers it)
  pdnet.ini                    settings (copied into your game folder)
  pdnet\Microsoft.Xbox.Services.dll   the pdnet build
  EOSSDK-Win64-Shipping.dll    Epic Online Services runtime
  eos.ini                      connection settings for the test (shown as "eos" if Windows hides
                               file extensions). Please don't share it further.

Requirements
------------
- Windows 10 or 11, 64-bit.
- Your OWN unpacked copy of Phantom Dust (the folder containing PDUWP.exe and AppxManifest.xml),
  version 1.3.25854.2.
- Developer Mode ON:  Settings > (Update & Security >) For developers > Developer Mode.
- Internet access.
- BOTH players must use THIS kit (stage C). Stage B and stage C can't play each other.
- Port forwarding is NOT needed. If you set some up for an earlier test, you can leave it or remove it.


STEP 1 - Install (each tester, once)
------------------------------------
1. Close Phantom Dust.
2. Unzip the whole kit into a folder (right-click the zip > Extract All...). Make sure eos.ini is in it.
3. Right-click install.ps1 > "Run with PowerShell".
   - Enter the path to your game folder.
   - Enter a player name, DIFFERENT from the other tester's.
   - Answer "y" to "Keep it running in the background?".
   (Or from a PowerShell window:
    powershell -ExecutionPolicy Bypass -File install.ps1 -GameFolder "D:\Games\PhantomDust" -Name "Alice")
   It backs up everything it changes into <game folder>\_pdnet_backup. If an earlier pdnet kit is installed,
   or you switched to Teredo, that's fine: this kit replaces it.
4. Start "Phantom Dust (pdnet test)" from the Start menu.
5. Check: the game reaches the main menu WITHOUT asking you to sign in to Xbox, and your profile loads.
   If it hangs or crashes here, stop and go to STEP 6 (send the log).


STEP 2 - Host creates a match (tester A)
----------------------------------------
1. Go to Multiplayer and create / host a match.
2. You should get to the lobby screen with yourself in it.
3. Note the time and leave the lobby screen open.
Either player can host. If you have time, repeat the whole test once with the other player hosting.


STEP 3 - Second player finds the match (tester B)
-------------------------------------------------
1. Wait about 15 seconds after the host's lobby is up (new lobbies take a few seconds to become findable).
2. Go to Multiplayer and use the option that searches for / lists matches.
3. Write down: is the host's match listed? Under what name, with how many players (e.g. 1/4)?
   If the list is empty, search again once or twice, 10 seconds apart.


STEP 4 - Second player joins (tester B)
---------------------------------------
1. Pick the host's match and join it.
2. BOTH testers write down:
   - Does the joining player reach the lobby screen?
   - Do both players appear on BOTH screens, with the right names?
   - Roughly how many seconds it took.


STEP 5 - Play, then leave and rejoin
------------------------------------
1. Host: start the match and play for at least a few minutes.
2. Write down how it FEELS, compared with Xbox Live games you've played before:
   - rubber banding (players jumping back), projectiles that vanish or don't hit, hits that register late
   - is it better, the same, or worse than before? Steady, or worse at times? Note the time of bad moments.
3. After the match, note where you end up (lobby? menu? error message?).
4. Joiner: leave the lobby. Host: does the joiner disappear from your lobby screen?
5. Joiner: find and join the host's match again. Does it work a second time?
6. Optional: leave the lobby open for 30 minutes with both players in it, then start another match.


STEP 6 - Send the results (each tester)
---------------------------------------
1. Close the game.
2. Right-click collect-log.ps1 > "Run with PowerShell". It saves pdnet-log-<PC name>-<date>.txt on your
   Desktop and shows a short summary.
3. Send that file, plus your notes from steps 2-5, the player names you both used, the countries/regions
   you're in, and wired or Wi-Fi. Screenshots of the lobby screens are welcome.

The log records the connection quality between you every 10 seconds ("p2p: link ..." lines: ping, jitter,
lost pings and traffic). That's what will tell us where any lag comes from, so please always send it.


If something goes wrong
-----------------------
- "eos.ini is missing": put the eos.ini you were sent next to install.ps1 and run it again.
- "Developer Mode is off" / "Missing Microsoft.VCLibs": follow the message, then run install.ps1 again.
- The game drops you from the lobby when you alt-tab: run nosuspend.ps1 once, then restart the game.
- Want your normal setup back: close the game and run uninstall.ps1 (restores your original files;
  local saves are kept). Or, to go back to Xbox Live with Microsoft's Teredo networking but keep local saves,
  edit pdnet.ini in the game folder:  xbox=on  eos=off  network_mode=teredo


Settings (pdnet.ini in the game folder; restart the game after editing)
-----------------------------------------------------------------------
display_name=...      your name as others see it (max 32 characters)
xbox=off              pdnet replaces Xbox Live
eos=on                Epic Online Services for lobbies and matches
network_mode=eos      matches through Epic's P2P service (no port forwarding)
p2p_relay=allow       Epic relays when needed (force = always relay, for testing only)
test_peer=off         debug only; leave off
