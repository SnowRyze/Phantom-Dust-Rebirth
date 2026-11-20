# Character select rework (livecharasel, menu 46)

Work by eradication0 (Nuion), started 2026-11-20, for a pull request to SnowRyze/Phantom-Dust-Rebirth.
Change notes also go in [`HANDOFF.md`](HANDOFF.md)'s change log.

## Goals

1. **X (pad) / x (key) toggles monsters**: the game only swaps the names; the portrait must swap too.
2. ~~Better portraits for the 11 player characters~~: dropped after the first test (the user prefers the game's own).
3. **Y opens an "Advanced" list**, with a Y entry in the bottom button bar.
4. **Advanced list contents**: every playable character once per movement type. First test: *Freia (jump)* and
   *Freia (dodge)*. Picking a jump entry shows 12 colour rows: 4 colours x 3 jumps (Meister Jump, Freia Jump,
   Player Jump), named "<colour> - <jump>", e.g. "Grey - Meister Jump". Dodge = Edgar's cartwheel (movement skill 751,
   Edgar's motions 33-39). Must work online.
5. Everything documented here and in HANDOFF, then a pull request.

## Status

| Part | State |
|---|---|
| Monster portrait art (11) | Done: `tools/charsel/` -> `kit/pdnet/charsel-portraits.bin`. Player art was tried and reverted (test 1). |
| MNet.alr copy with the art (MakeMnetCopy) | Done. Checked off-line: the C++ output is byte-identical to a Python reference whose sprites were decoded and viewed. Not yet seen in game. |
| Monster portraits on X | Test 1: not shown (the name line is `\a130c%s\f`, so the name was never read). Fixed; the grid's 11 places map to their monsters (kMonsterFor). |
| Y: Advanced on / off, heading, button bar entry | Test 1: no reaction, no bar entry. Now also the Y key from the game's key handler, the bar's text is logged, `$MONSTER_HINT` handled, a 2 s status line while the screen is open. |
| Advanced list contents (Freia jump / dodge, 12 colour rows) | Not started: needs a menu_debug log of the screen (below). |
| Movement variants as characters (charaset, Player folders, online) | Not started: design below. |

## How it works

### Files

- `tools/charsel/source/characters.png`, `monsters.png`: the guide images (1000 x 559) the portraits are cut from.
- `tools/charsel/portraits/*.png`: 22 portraits, 134 x 96 RGBA (edit by hand if a crop is off). Only the 11 monsters are packed.
- `tools/charsel/make_portraits.py`: crops the guides (unless `--pack`), packs `kit/pdnet/charsel-portraits.bin`.
  `dxt.py` is the DXT5 encoder from the phantom_dust_stage Blender add-on.
- The .bin sits directly in `kit\pdnet\`: pdlaunch's updater copies only files, not folders.

### MNet.alr (texture slot 3, "MNET")

Layout (2017 release, 863232 bytes): header (0x14 data size), texture table at 0x28 (0x15, 12 entries of 0x1c from
0x34: +4 offset in the data, +0xd format (0x0f DXT5), +0xe log2 width << 4 | mips, +0xf log2 height), then the
sprite record at 0x198: type 0x10, size 0x1640, +8 texture count (12), +0xc sprite count (84), "MNet", 12 texture names
(0x20 each), 12 texture infos (0x14 at 0x338: u16 w, u16 h, 4 bytes, u32 sprites on it, u32 format), 84 sprites
(0x3c at 0x428: u32 texture, name[32], float u0 v0 u1 v1, u32 w, u32 h), the end record (type 0, size 8), padding
to the data at 0x2000.

Sprites used here: 0-10 window_chara (small player portraits), 11-26 window_chara (small monster portraits),
38-48 player_00..player_10 (the 134 x 96 portraits, textures 9-11), 84+ (pdnet's monster portraits).
Portrait order = the game's character order: player(Alpha), Edgar, Freia, Meister, Chunky, Cuff Button, pH, Know,
Tsubutaki, JD, Sammah.

The copy: MNet0800 (texture 9) 256 -> 1024 wide (own pixels on the left, the monster art on the right; its sprites'
u scaled), 11 monster sprites appended (the record grows by 0x3c each into the padding), counts and offsets fixed. It's made once
per start in `LocalState\pdnet\MNet.alr` and handed to the game by the CreateFile2 hook (as FDLogo.alr).
`charsel_portraits=off` in pdnet.ini turns all of this off.

### Script facts (livecharasel_en.ssb)

- Script call words: `op 0x01 | nargs << 8 | index << 16 | group << 28` (index 12 bits). Group 2 entry = menu command + 64,
  so `0x209f0501` is menu command 95 (draw sprite: sheet, sprite, x, y, colour) with 5 args; group 4 #571 stores the pick.
  The screen calls menu command 95 seven times (portraits, cursor, etc.).
- Labels (base-40 names): `_DRAW_PLCOLO` (the COLORS rows), `mCHSEL`, `dCHSEL`, `aCHSEL_TA`, `aCHSEL_TB`, InitMenu, QuitMenu.
- Button bar `$MONSTER_HINT` = `\a6#Select \a580r\a4#Toggle Monsters \a2#Confirm \a3#Back`. Icons: `\a2#` A, `\a3#` B,
  `\a4#` X, `\a5#` Y (the lobby's Invite).
- Name line: `\a130c%s`, colour rows `COLOR \a1f%d` (ColourNameText).

## Test 1 (2026-11-20, build from 70ab91c)

- The screen is a grid of all 11 portraits at once (sprites 40, 38, 39 in the top row at y 65; 41-44 at y 163; 45-48 at
  y 262), plus 67 (cursor frame) and 71 / 72 (name plate ends). Toggle Monsters draws the same 38-48.
- The patched MNet.alr loaded fine (new player art showed). The user wants the game's own player portraits back.
- Monster portraits didn't show: the name line's format is `\a130c%s\f`, the code compared it to `\a130c%s`.
- Y did nothing and no bar entry showed; the log had no Y line. Unknown yet whether AnyPadHolds sees the pad's Y on
  this screen or what text the bar gets: both are logged now.
- Grid place -> monster, from the guide images and the logs (Freia -> Vestio, player -> Ommato): player Ommato,
  Edgar Anthro, Freia Vestio, Meister Guard, Chunky Andro, Cuff Button Ceno, pH Partheno, Know Catoptro,
  Tsubutaki Gyne, JD Claustro, Sammah Scoto.

## Test 2 (2026-11-20)

- Monster pictures showed only as a faint layer behind the players' portraits: the grid is drawn twice from sprites
  38-48 and only menu command 95's layer was swapped. Now the loaded sprite table's records are switched (ShowGrid).
- Y worked from the keyboard only: the pad check used 0x80 (D-pad down); Y is 0x20.
- The bar arrives as `$MONSTER_HINT`; the Y entry is inserted after the English text. Advanced: "Y All Characters",
  no X entry. Heading back to "Online" when leaving Advanced.
- Advanced test list: Freia (jump) at Freia's place, Freia (dodge) at player's place, the rest empty.

## Testing (Experimental build from this branch)

1. Character select (Lab > Test Arsenal, or a lobby): the game's own player portraits.
2. Press X: monsters' names *and* portraits (Vestio ... Scoto). Mechano, Euroto, Hedono, Germano and Belono have no
   art yet and keep the old picture.
3. Press Y: heading ADVANCED, bar shows "Y Normal"; Y again goes back.
4. Send `pdnet.log`. Lines to look for: `character select portraits: ...`, `character select draws sheet 3 sprite ...`,
   `character select: Advanced list`.
5. For the next step, also once with `menu_debug=on` in pdnet.ini: open the character select, move through a few
   characters, press X twice, Y twice, pick one, then send the log (menu-debug lines for menu 46 show which commands
   build the list and handle input).

## Plan for the Advanced list and movement variants

**Menu.** The list is the game's own screen with pdnet in front of it while `g_charselAdvanced` is on: the names,
portraits and colour rows are swapped as they are drawn (like ColourNameText), the cursor is limited to the variant
entries, and the pick is rewritten before g4 #571 stores it. The 12 colour rows need the COLORS window's row count
(menu commands 33/36/40 set windows up elsewhere); the menu_debug log shows which command it is here.

**Characters.** Movement type is per charaset record (+0x36: 750 jump, 751 cartwheel; no per-colour fields) and the
jump itself is the character's motions 33-39 (see the Blender add-on notes: jumpers vs cartwheelers). So each variant
is its own character slot: a charaset record copied from the base character with +0x36 set, and a Player folder whose
ALRs are the base character's with motions 33-39 taken from Meister / Freia / Player (or Edgar for the dodge). The
phantom_dust_stage Blender add-on already does that swap (characters.py, char_build.py, charaset.py). pdnet would build
these at start the way MNet.alr is built (copies in LocalState, CreateFile2 hook), so no game file changes.

**Online.** The match setup carries the character as `colour << 16 | character` (+8 of each 0x48 player entry). A
variant is a new character number past the game's last (the character table's size and any fixed-size arrays have to
be found first). Both players need a pdnet with the variants; the version check / lobby should refuse a variant pick
against an older pdnet. To be tested with two PCs.
