PHANTOM DUST (2017 PC) - pdnet direct peer-to-peer TEST BUILD
================================================================

What this is
------------
An experimental replacement for the game's Xbox Live peer-to-peer layer (Teredo / Xbox
"Secure Device Association"). Players connect to each other over plain UDP instead.
Lobbies and matchmaking still go through Xbox Live, so you still sign in with an Xbox account.

It also stores your saves locally instead of in Xbox cloud storage. That fixes the
"retrieving profile" hang on unpacked copies. Your Xbox cloud progress will NOT show up.

This is a first test. Peer-to-peer connections have not been tried between two PCs yet.
That's what you're helping with. Thank you!

This kit does NOT include the game. You need your own copy.

Requirements
------------
- Windows 10 or 11, 64-bit.
- Your OWN unpacked copy of Phantom Dust (folder containing PDUWP.exe and AppxManifest.xml),
  version 1.3.25854.2. The Microsoft Store install itself can't be modified (it is encrypted).
- Developer Mode ON:  Settings > (Update & Security >) For developers > Developer Mode.
- An Xbox account that is DIFFERENT from the other tester's.

Install
-------
1. Close Phantom Dust.
2. Right-click install.ps1 > "Run with PowerShell", and enter the path to your game folder.
   (Or: powershell -ExecutionPolicy Bypass -File install.ps1 -GameFolder "D:\Games\PhantomDust")
   It backs up everything it changes into <game folder>\_pdnet_backup.
3. Start "Phantom Dust (pdnet test)" from the Start menu.

Network setup
-------------
- SAME home network (easiest, do this first): nothing to set up.
- Over the INTERNET (for now): the HOST must forward UDP ports 43210 and 43211 on their router
  to their PC, and set  advertise_ip=<host's public IP>  in pdnet.ini (in the game folder).
  The player who joins doesn't need to change anything.
  (Automatic internet connectivity without port forwarding is not part of this build.)

The test
--------
1. Both players: start the game and sign in.
2. Host: Multiplayer > Xbox Live > create a lobby / host a match.
3. Other player: find and join that lobby.
4. Try to start a match and play a round.
5. Both players: run collect-log.ps1 (copies pdnet.log to your Desktop) and send that file,
   plus a short note: what you saw, at which step it stopped, same network or internet.

Useful lines in the log: "sda: connecting to ...", "sda: connected to ...",
"sda: inbound connection from ...", "TIMED OUT", "packet from unknown peer".

Uninstall
---------
Close the game, right-click uninstall.ps1 > "Run with PowerShell", enter the game folder.
Original files are restored from _pdnet_backup. Local saves are kept.

Settings (pdnet.ini in the game folder)
----------------------------------------
network_mode=direct   direct = pdnet;  teredo = original Microsoft networking (saves stay local)
advertise_ip=auto     or the host's public IP for internet play with port forwarding
game_port=43210       must stay 43210 (matches the game)
control_port=43211    pdnet handshake/ping port
test_peer=off         debug only; leave off
