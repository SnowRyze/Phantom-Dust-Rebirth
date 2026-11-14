# Conceptual branch notes

Very experimental work. This branch lives in its own folder, `D:\PhantomDust-Conceptual` (a git worktree of
`D:\PhantomDust-Dumped-NoMovie\_netmod`), so a master session and a Conceptual session can run at the same time.

## Things to know
- **One game install.** Whichever kit's Setup.bat was run last is the one the game runs, so testing a Conceptual kit
  and a master build means switching between them with their Setup.bat. To test a Conceptual kit: unzip it to its own
  folder and run its Setup.bat. To go back to the normal builds: run Setup.bat in the Experimental 72 folder again.
- **Merging master in happens here.** Git won't check out Conceptual in the main folder while this folder has it, so
  merges are done from this folder: `git merge master`, then `git push origin Conceptual`. Changes go from master into
  Conceptual only, never back.

## Rules
- Commit only to Conceptual; push only to `origin Conceptual`. Never force-push.
- Kits: `.\build.bat conceptual "what it tries"` saves `PhantomDust-pdnet Conceptual <N>.zip` on D:\ and, once the code is
  committed and pushed, publishes a pre-release `conceptual-<N>` with the asset `pdnet-conceptual-<N>.zip`. Installed
  Conceptual kits update from those only. Other kits ignore that asset name, and a Conceptual kit ignores update_channel.
  Off switch: Options > Updates (`conceptual_updates=off`). Never `build.bat experimental` or `promote` here.
- Notes for this work go in this file, not in HANDOFF.md; tools\kit-counter.txt stays untouched.
- `deps\eos-sdk` and `private\` are links to the main repo's; never commit private\ files, eos.ini, links.ini or game
  files.

## Feature assessment (2026-11-03)
Read-only: from the code, the game's data and Ghidra. No code written, nothing tried in the game. Sizes are relative
effort, not time.

### How matches sync
PCs don't exchange button presses. Each PC runs its own player and sends the others updates through 26 streams, table
at 0x1403fb4d0 (0x28 bytes each: +0 size, +4 sequenced/resent, +0x10 send, +0x18 receive, +0x20 name): PlayerPos,
CardKind, FireStartReq, FireReq, SkillActionCtrl, SkillActionEnd, Stage (2), CapsuleData, EnvCapture (2), CapsuleSteal,
CapsuleMove, PowSteal, Condition, DrawTimer, PlayerCaptureReq, PlayerCaptureAns, PlayerRevive, NoDmgTimer, DmgEffect,
Warp, ReadyStart, KilledCount, SkillHit (2). Per-frame send: FUN_14027abe0 (one packet per other slot, through
FUN_140125430).
- Each PC already gets the others' positions and hands, and pdnet carries all of it. CardKind's receive, FUN_14024dd90,
  writes four values (the four hand slots) into that player's status record.
- A replay is one PC's version of the match; PCs can disagree on bad connections ("cross lagging", HANDOFF section 11).
  Custom rules that decide a winner need the host to make the call.
- Waiting: FUN_14012bbb0 skips a frame when an active peer's last data is over 60 frames (0x3c) old, in network modes
  only (game mode 4 or 5). So PCs wait for stale peers, which is the freeze in HANDOFF. A spectator must not count as a peer.

### Each idea
| Idea | Possible? | Size | Main catch |
|---|---|---|---|
| Dynamic arsenal cases | Very likely | Small | Does the case's school count affect anything besides the editor limit? |
| Lobby settings | Very likely | Small to medium | All PCs must use the same values; in-lobby rule/type changes have crashed before |
| Dead camera | Very likely | Medium | Showing enemy hands to a dead player allows callouts over voice |
| Draft / Sealed | Likely | Sealed medium, Draft large | Must never alter the real save; the pick screen is the most work |
| New game modes | Depends on the mode | Small to large | Variants are easy; objective modes need host-decided scoring |
| Theatre | Likely | Large | Viewing a match with no local player is unproven |
| Spectator | Possible | Large | Full 2v2 matches have no free player slot |
| 3v3 or more | Possible but unlikely | Very large | Four players are built in throughout the game |

### How each would work
- **Dynamic cases:** replace the fixed limit with a count of the schools in the arsenal. Limit: case +0x50 (arsenal
  entries are 100 bytes, list at *(save + 0x10a*0x38)); the editor shows "You are limited to %d schools". Open: a maximum?
  What happens to buying and selling cases?
- **Lobby settings:** use the route of the host's nametag setting (PD_BODYTAGS, mpsd ApplyHostRules) and the spare lobby
  window (HANDOFF backlog 4). Custom matches are Unrated. Earlier in-lobby change attempt: HANDOFF backlog 3.
- **Dead camera:** a camera that follows any player, plus a freecam. The view is rebuilt, not their exact screen, and must
  snap back on revive. The local slot DAT_1404bbe00 is also used by the network send code, so flipping it isn't enough.
- **Game modes, easiest first:** rule variants (health, aura); kill-count or respawn modes (KilledCount, PlayerRevive
  exist); objective modes or uneven teams such as 3v1; team switching mid-match.
- **Draft / Sealed:** Quick (rule flags 0) hands out premade arsenals and Test Arsenal builds one, so the battle side is
  mostly there. Sealed first (a shared seed gives everyone the same packs), then Draft (live picks). Packs and rarities
  are a design job.
- **Theatre:** pdnet saves all match traffic and the match setup to a file, then plays it back. The setup needs saving
  because stage and rule travel in the host's 0x80-byte peer message. Forward only at first; rewind means restart and
  fast-forward.
- **Spectator:** Theatre fed live. Players' PCs send a copy of their traffic to the spectator, who needs no game slot and
  sends no acks. This also gives a tournament delay. Until then, a caster can stream a dead player's camera.
- **3v3:** the tables hold four entries and sit next to other data, so they can't grow in place. References: DAT_1404bc9d8
  about 500, DAT_1404bc608 about 180, DAT_1404bbe00 about 125. Masks (0xf), HUD, lobby and mesh loops also assume four.
  15 links instead of 6 makes cross lagging worse. Try uneven teams inside four players first.

### Suggested order
1. Three offline tests in Test Arsenal: follow the AI with the camera, record a match to a file, try a match with no
   local player. The third decides whether Theatre and Spectator are realistic.
2. Dynamic cases, lobby settings, dead camera.
3. Game mode variants, then Sealed, then Draft.
4. Theatre, then Spectator.
5. 3v3, only if everything else is done and a small test looks promising.

Anything that changes match behaviour needs every player on the same kit, and real tests need at least one other person.

## Log
(Newest first: what was tried, what happened.)
- 2026-11-14: Theatre film controls, from the user's Halo 3 film-controls card, each with a key (Conceptual 32):
  A / Space play-pause; RT (light pull slow motion x0.25, full pull fast forward x4) or Z / C held; Y / V flying cam or
  player cam; B / H HUD; X / F1 the controls overlay; D-pad / Q E switch players; left stick / WASD move; right stick /
  arrows look (orbit in player cam, R3 / T resets); RB LB / R F up and down; LT / Shift thrust; L3 / Ctrl held pan.
  Time control: the game's per-frame step FUN_1401c7630 (called at 0x1401c865a, from pdProcessManager's loop) runs every
  task group's update with FUN_1401cbd40, the world groups (1..0x1f) only when DAT_140530034 & 0x20001 is 0 (bit 0 = the
  game's own pause). Draws (FUN_1401cbcd0), and so the camera, run either way. HookedStep sets bit 0 for the step while
  paused, and on some frames for slow motion. For fast forward it runs the step several times a frame. The film is fed
  per step from a film clock that runs at the same rate (g_filmClock), not wall time. The overlay is drawn in the HUD
  hook with the nametag text call (FUN_1402477e0) at about x 24, y 300; it shows the time, PAUSED / SLOW MOTION / FAST
  FORWARD, the camera and the watched player. Not tested yet.
- 2026-11-14: Conceptual 30 test: the film decks loaded (both sides played with the film's skills), but a 3-player film
  showed only 2 players, and playback stopped 0.2 s in when Condition (stream 14) faulted. Its data names the target
  player (byte 2), here film slot 2, which didn't exist in the 2-player battle. Conceptual 31: every film player keeps
  their own slot. The battle setup (FUN_140128990) copies 4 entries of 0x48 (a player is there when its first int isn't 0),
  and the scoreboard battle fills 2. HookedTestStart fills entries 2 and 3 as copies of the opponent's entry (the AI's) with
  the film player's character, Aura rate 1.0 and deck counts built from the film. The film's names go into the name
  table (DAT_1404bca30) when playback starts. Not tested yet: the game mode stays One-on-One, and the extra players share
  the opponent's AI data (driven by the film anyway).
- 2026-11-14: Conceptual 29 crashed in film playback: DmgEffect (stream 20) faulted 24 s in (effect 0, type 0), then the
  game crashed at PDUWP+0x18c383 (FUN_14018c360, an effect instance with no resource data). Two more findings: both
  sides played with this player's own arsenal, so the film's skills were never loaded; and a sequenced stream's number
  isn't always new (DmgEffect kept #196915 while its data changed), so the "play only when the number moves on" filter
  dropped real messages. Fixes (Conceptual 30):
  - Film decks: film::Load gives each film player a deck: the recorded arsenal (version 2), or else the skills seen in
    their hand (CardKind, 4 shorts) plus the ones they fired (FireStartReq, u16 at +16; the hand alone missed most), repeated
    to 30. The scoreboard battle's two FUN_14014f830 calls (0x1402148da player, 0x14021491f opponent) are hooked to build
    each side's deck counts from that, so the battle loads them. Streams 2+ of a film player with no deck are skipped.
  - DmgEffect isn't played (the battle draws its own hit reactions). ReadyStart still skipped.
  - Copies: a film slot's stream is skipped only when it repeats the last played one exactly (number and data).
  - That film had 3 players (this PC was slot 2); only the first two are shown.
- 2026-11-14: Theatre playback plays skills. The first test (Conceptual 28) moved the players but showed no skill
  animations or sounds: only PlayerPos and CardKind were fed. Now every stream goes to its own receive function, read from
  the stream table (0x1403fb4d0 + i*0x28 + 0x18) and checked against the known addresses; all take (slot, state) with the
  data at state+0x40. Sequenced streams are resent until acknowledged, so each film slot's stream plays only when its
  number moves on. ReadyStart (22) is skipped: the battle has already started. The end log line counts skills started,
  fired, actions, hits and damage effects. Also: in film review the watched player gets a nametag too.
  Known limits: slot numbers inside the data (hit targets, steals) assume the film's slots match the battle's.
- 2026-11-14: Theatre playback, first version. A on a film loads it (film::Load decodes it, bounds-checked) and starts a
  Test Arsenal battle at once (no picks): the film's stage and layout, its characters when recorded, this player's current
  arsenal on both sides. At the battle's start every player is driven by received data and the film's PlayerPos and
  CardKind records go to the game's receive code at their recorded times; the 24 other streams (skills, hits, ...) are only
  counted for now. Watching uses the dead camera (follow anyone, orbit, free camera, their HUD). Film setup snapshot
  version 2: each player's character (DAT_1404bc420 + slot * 0x48 + 8), arsenal (deck counts DAT_140585fe8[slot] plus the
  hand) and Aura rate. Test Arsenal battles are no longer recorded. Not yet tried in the game.
- 2026-11-14: Conceptual 27 tried: A picks the highlighted film on either page (page 2 rows 0-6 the right films, empty
  rows 7-9 say no film there). The Theatre list is done apart from playback.
- 2026-11-14: Theatre tried on Conceptual 26: 17 films listed, paging both ways; A logged nothing (it never reaches session
  command 154 here). Now A is read in the button handler: the film on the highlighted row, the window's cursor at +0x12
  (window = manager +0x10402; FUN_14024b000 moves it). Seen there too: +0x16 rows, +0x18 rows shown and +0x1a the top row,
  so a window can scroll natively (a way to list all 100 films without pages). Not yet tried in the game.
- 2026-11-14: 100 films kept (was 25, the user). The Theatre lists all of them, 10 rows at a time: LB / RB (Page Up / Page
  Down, Q / E) turn the page, the hint says page N of M; the rows are buffers the window keeps reading, so a page only rewrites
  them. The list reads only each film's first 256 KB (setup) and last 28 bytes (end record: length), not whole files.
  Not yet tried in the game.
- 2026-11-14: Theatre menu, first part (the user: Theatre replaces Trade, a film list before the viewer). The multiplayer
  menu's Theater item (livetrade 40 from livemain 36) now opens the Settings screen (liveoption 68) as THEATRE: its rows are
  the newest 10 films (film::List: date, game type, length), each row's hint the stage and players; A logs the pick
  (playback not built). Title, button bar (Select / Watch / Back) and the Save question switch with it. Not yet tried.
- 2026-11-14: Theatre step 2, the film reader (toolsilmread.py, py -I toolsilmread.py <film> [--stream NAME]). Packet:
  7-byte mesh header (u16 sequence, u8 type 0x40 data / 0x01 heartbeat, u32 length), 8-byte message header (u16 length, 0x1200,
  u16 kind, u16 sender slot); kind 8 = the per-peer block (u64 mask: bit N stream N, bit 26+K ack of the K-th sequenced stream;
  u32 slot, u32 frame/2, u32 lag, u32 FUN_140184730), then the streams in order (sequenced ones with a u32 number first), a
  u32 per ack, the rest voice. Kinds 4, 10 and a kind-8 mask with its top bit set: end-of-match messages. All 15 online films
  decode with nothing left over; the crashed game's film was empty (bad magic, refused cleanly). The recorder now flushes each
  write, so a crash loses at most a second. Next (the user): the Theatre menu replaces Trade, with a film list before the viewer.
- 2026-11-14: Conceptual 24 tried (Test Arsenal, F9): followed the AI 37 s, free camera, off at the battle's end, no crash;
  the orbit controls turn the right way (the user).
- 2026-11-14: crash on Conceptual 23 (installed in the "Conceptual 22" folder) while following the AI (Test Arsenal, F9): PDUWP+0x15145e in the HUD draw, reading
  DAT_1404bcb50[object +0x13f20] +0xb8 when the HUD's player's camera block says a lock is on (+0x13f1c); that block is kept
  only for this PC's own player, and on the AI the entry was empty. Now, for the swapped HUD draw, a lock that doesn't lead to
  a real target is off. Dump: CrashDumps\PDUWP.exe.46292.dmp. Not yet tried in the game.
- 2026-11-14: follow camera controls (the user): the view stays on the watched player, as a player's own camera does, but the
  right stick (or Q E, R F) swings it round them and up / down; right stick click (or C) puts it back behind; a new player
  resets it. Not yet tried in the game.
- 2026-11-14: tried on Conceptual 22 (Test Arsenal, F9): facing read as way 0 after 5 s, camera shape 51.2 behind, 39.4 up,
  looking 5.5 ahead at 8.4 up; followed the AI, free camera, and off when the battle ended ("the battle is over").
- 2026-11-14: the dead camera now runs only while the battle does (match state 6); it stayed on over the results screens
  (states 12, 9, 10, 11) until the next match. Log: deadcam: off (the battle is over). Not yet tried in the game.
- 2026-11-14: the follow camera sat too close (the user). Its distance was the eye's offset along the facing only, which shrank
  whenever the camera looked at a target off to one side. Now the full horizontal distance, averaged, from frames where the
  camera is behind the player along its facing (log: follow camera shape). Not yet tried in the game.
- 2026-11-13: recorder tried online on Conceptual 20 (the user vs Motley, 2 players, 5 matches): a film per match, 1.6 to 5.7
  min, 142-184 KB a minute (budget 1 MB), nothing dropped, the files read back cleanly. Snapshot: game type 1, rule 0, stage 5,
  this PC slot 1, both names and teams, the arsenal. The 5.7-min film: 5266 packets out, 5256 in, mostly 87 bytes; the game
  frame counter moved 4377 in 340 s (about 13 a second online, 30 offline: to understand before playback).
- 2026-11-13: Theatre, step 1: the film recorder (srcilm.cpp; the user's REPLAY_SYSTEM_SPEC.md, strategy B: no server here,
  so a film is one PC's view). Each match: LocalState\pdnetilmsilm-<date>-<time>.pdfilm with every game datagram sent
  and received over EOS P2P (p2p.cpp), the time and the game frame, match states, and a setup snapshot at state 6 (type, rule,
  stage, layout, slots with name / team / health, this PC's arsenal). Written about once a second by a background thread,
  8 MB buffer cap (excess dropped and counted), newest 25 kept. Not the direct network mode. Other players' arsenals: not
  found yet (needed for playback). Not yet tried in the game.
- 2026-11-13: the log has an online run on Conceptual 16 (3-player Battle Royale, 01:11): the dead camera came on when the
  player went down, followed two players and the free camera, no errors. Skills not loading on the watched HUD, likely cause:
  only CardKind (+0x350, +0x378) comes over the network; the HUD shows the name of the player at +0x384 (card's source) in
  place of the skill when 0 or more, never set on another PC's player. Now -1 on such a player for the HUD draw. Unconfirmed.
- 2026-11-10: the follow view looked wrong (the user: own camera offset). Now from behind the watched player along their
  facing, object +0x12e68 (16-bit angle, in PlayerPos +0x18): how it maps to a direction is learnt from this player's own
  camera while it stands (8 readings, best match after 120 frames; log: facing angle read as way N), with the camera's shape
  measured the same way. Skills sometimes missing from the watched HUD: not understood yet; the hand fields are logged.
- 2026-11-10: following, the other players' tags were missing (the user). Now, whenever the view isn't the player's own,
  the game's tags are off for the HUD draw (its player's entries zeroed) and pdnet draws every player's tag from the camera
  actually used (a viewer that is nobody), except the watched player. Colours: dead, ally (this player, Tag partner), enemy.
  Not yet tried in the game.
- 2026-11-10: tags fixed (the user), but the down player's own tag was missing: the HUD never tags its own player and skips
  bodies unless body tags are on. Now drawn by pdnet after the HUD, the HUD's way (FUN_1402477e0 at the entry + (0x140, 0xe6),
  name DAT_1404bca30 + slot * 0x41, FUN_14028caa0), dead colour once down. The free camera's projection uses a viewer that is
  nobody, so it includes this player. As a body: only testable online. Not yet tried in the game.
- 2026-11-10: dead camera nametags (the user): in the free camera they floated, following a player there were none. The HUD
  places tags from the local player's camera block (+0x13f10, entries at +0x80, from FUN_140253c00 with that block's view).
  Now, while the view isn't the player's own, a stand-in block with the frame's real view (0x140584f40) is worked out the
  same way and lent to the HUD's player for the draw. Not yet tried in the game.
- 2026-11-10: PAUSED, lobby settings, nothing built yet. The user's choices: the host sets starting health, starting Aura
  level and an Aura refill multiplier (on top of the dynamic case rate) for everyone; no stage pick. Rating: custom matches
  rated only with a password set in the admin tool (outside this repo; HANDOFF 2026-10-28), on their own scoreboard named
  "Tournament", renamable from the admin tool if possible. To find out next: how the admin tool works and talks to EOS, a new
  EOS leaderboard / stats for Tournament (made in the Epic portal), and where the game sets starting health and Aura.
- 2026-11-10: dead camera tried offline on Conceptual 13 (Test Arsenal, F9): followed the AI, free camera, back to following,
  no errors; the HUD showed the AI's (the user). Free camera directions and the follow angle fine too. Online not tried.
- 2026-11-10: merged master (Public channel, skill previews, Lab Player 2 fix); Conceptual kits keep their own channel in
  master's update code (tag conceptual-<N>). Dead camera, first version (src\labtests.cpp): when this player is down (health
  record +0xc <= 0) in any match, the view follows a teammate (Tag Team) or anyone standing, with their HUD (the HUD draw at
  0x14047b8f0 runs with the local slot swapped), or a free camera. Arrows / D-pad pick and toggle; WASD QE RF or sticks and
  LB/RB move the free camera. F9 or View + D-pad Down pretends to be down in Test Arsenal. Not yet tried in the game.
- 2026-11-07: the slot label above an arsenal's name ("<prefix>NN") is gone, the user's request: FUN_14016d5f0 draws it only
  when its 6th argument is 0 or more; its three calls now pass -1. Story profile too. Not yet tried in the game.
- 2026-11-07: Max Credits & Skills gives an Arsenal Case each press, up to 16 (the user's choice). The main shop (menu 6)
  also offers one plain case: its stock commands (group 3 #310, #347) are wrapped. Multiplayer profile only. Not yet tried.
- 2026-11-07: the buy screen still said "1-School Arsenal Case": the script's texts are longer strings (the row holds the
  price too). The dialogs are now matched whole, and the row is changed as drawn (menu command 23) without its school
  number. Not yet tried in the game.
- 2026-11-07: confirmed in a Test Arsenal battle (the user; read from the running game's memory, read-only): the player's
  3-school arsenal refilled at 0.7x (DAT_1404bc958), the opponent's 2-school copy at 1.0x. The limit and its pop-up work.
  1.3x for a 1-school arsenal not yet seen.
- 2026-11-06: school limit pop-up. A refused 4th school gives no sign (the user), so it now uses the game's "3 of the same skill"
  pop-up (the -3 refusal) with its text swapped to the school limit, English only. Not yet tried in the game.
- 2026-11-06: school limit fix. The number followed the arsenal, but a 4th and 5th school went in: the save check's counters
  weren't current. Now a skill making a 4th school is refused as it's added (FUN_140159db0, its own -1 refusal), and the save
  check counts the arsenal itself. Not yet tried in the game.
- 2026-11-06: the case's number now also follows the arsenal live in the editor, as a visual indicator (the user's request).
  The limit of 3 moved to a wrap of the editor's check (0x140216cc0), which runs when saving ("too many schools"). The
  "School Limit" text is gone. An empty arsenal counts as 1. Not yet tried in the game.
- 2026-11-06: dynamic arsenal cases, the user's choices: the game's Aura rates, 3 schools at most, one plain case to buy.
  The school number (case +0x50) sets the editor limit and the Aura refill rate (1 school 1.3x, 2 1.0x, 3 0.7x). It now
  follows the arsenal in the multiplayer profile. Story unchanged.
  The buy texts are English only. Not yet tried in the game.
- 2026-11-06: offline tests passed on Conceptual 5 (the user: works; log and labtest.rec checked):
  - Update: Conceptual 4 installed build 5 by itself. As master notes, the keyboard needs the second start.
  - Camera: followed slot 1 for 27 s; its position, health and hand (it changed mid-battle) read fine.
  - Record: 447 frames in 14.9 s, 30 game frames a second, 200 bytes each, 5.9 KB a second (positions and hands only).
  - Play back: 447 of 447 frames with both players at +0x34 = 0, 0 faults; both ended at their recorded last position,
    and local control came back. So a match can run with nobody under local control, driven by recorded stream data.
  - Not covered: the other 24 streams (skills, hits), real network traffic, the HUD for another player, 2v2.
- 2026-11-06: Conceptual 5: F6-F8 never worked (1-4): the window gets no key events on this PC. Merged master's keyboard fix;
  the tests read pdlaunch's keys (KeysHeld), and also take View + D-pad (Left camera, Up record, Right play back).
  Not yet tried in the game.
- 2026-11-06: Conceptual kits get their own auto-update (the user's request; Conceptual branch only). Kits 1 and 2 can't
  update: install one built with it by hand once.
- 2026-11-06: Conceptual 2: in Conceptual 1 no test ran (log: labtest not installed); the camera call is a tail jump (e9) at
  0x14017d00e and the hook only accepted a call. Fixed. Not yet tried in the game.
- 2026-11-04: Conceptual 1, three offline tests in a Test Arsenal battle (src\labtests.cpp): F6 camera follow, F7 record,
  F8 play back with both players driven by the file (object +0x34 = 0). Built; not yet tried in the game.
- 2026-11-03: feature assessment written (above). Read-only, no kit built.
