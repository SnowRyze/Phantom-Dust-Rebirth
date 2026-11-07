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
