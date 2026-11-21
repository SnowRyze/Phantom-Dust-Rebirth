# Adding characters to the Advanced list

This is for adding characters to the character select's Advanced list (Y on the character select), for example
imported or modded ones. The menu, its pages, pop-ups, drawing layers, the pick, the online sharing and the lobby
banner already work for any entry in `variants::Characters()`. Adding a character means adding one entry and,
optionally, its pictures. Nothing in `gamepatches.cpp` needs to change.

Background: docs/CHARACTER_SELECT.md (how the screen and the variants work) and docs/HANDOFF.md (change log).

## What the Advanced list does with an entry

1. **Grid.** Every entry gets a place in a 6 x 5 grid (30 a page). With more than 30 entries the list has pages:
   LB / RB (keyboard Q / E) turn them and the heading shows `ADVANCED n/m`. `DrawAdvancedGrid`, `CharacterSelectButtons`.
2. **Pop-ups.** A opens Color, then Movement Type, then Animation Set (`OpenCharselPopup`, `DrawCharselPopup`). The rows
   come from the entry's `colours`, `variants::Dodges()` and `variants::AnimationSets()`. Up to 8 rows are shown.
   Characters of `Kind::Monster` only get the Color pop-up and are picked as the game's own record (no variant).
3. **Pick.** The pick plays as a "carrier": a charaset record the normal lists don't offer, one per lobby slot
   (`kCarriers`: Mechano, Euroto, Hedono, Belono). `ActivateVariant` patches the carrier's charaset entry in memory
   (hitbox, movement skill, movement stats, voice, exmot lengths). When the game opens the carrier's files, pdnet builds
   them for the pick (`BuildVariantFile`: the character's model with the dodge's and the set's animations moved onto its
   skeleton) into `LocalState\pdnet\variants`.
4. **Lobby.** In a lobby the pick is confirmed the game's own way (A on the grid and on its COLORS row). The game stores
   pdnet's value, and the pick is shared as the lobby member attribute `PD_VARIANT` = `1|slot|name|colour|dodge|set`.
   Other PCs look the character up **by name**.
5. **Banner.** The lobby list shows the entry's banner instead of the carrier monster's (`LobbyBanner`).

## Steps

### 1. The model files

`variants::BuildModel` reads the model through `ReadGameFile` (gamepatches.cpp), with paths relative to the game's
data folder:

- `Kind::Player` / `Kind::Monster`: `Player\<folder>\<folder><colour>.alr`, a battle model (130 animation runs, then
  the model runs with the body skeleton first). The animation runs are replaced, but the model must have a skeleton
  with the usual joint names, so the game's animations can be matched onto it.
- `Kind::Npc`: the model `<folder>` (e.g. `npc11a`) in the game's `Event\*.mk` bundles; `version` picks the
  nth different copy of it.

If imported files live somewhere else (for example `LocalState\pdnet\mods\...`), change `ReadGameFile` alone to look
there too. Every build goes through it.

### 2. The entry

Call `variants::AddCharacter` once at start-up, before the character select opens. A good place is the start of
`gamepatches::CharacterSelectHooks()`, or wherever the importer loads its list:

```cpp
variants::Character c{};
c.name = "My Character";          // shown in the list; must be the same on every PC (online picks go by name)
c.kind = variants::Kind::Player;  // Player (battle model in Player\), Npc (Event\*.mk), Monster (own record, Color only)
c.folder = "mod01a";              // Player\mod01a\mod01a0.alr ... (or the NPC model name)
c.version = 0;                    // NPCs only: which copy in Event\*.mk
c.record = 2;                     // charaset record whose hitbox to use (2 = Freia's); -1 = keep the carrier's
c.colours[0] = "Red";             // one name per colour file 0..3; leave the rest nullptr
c.voice = "BattleVo_Freia.bin";   // a voice bank the game has
c.portrait = 128;                 // MNET sprite numbers from tools\charsel\sprites.txt (step 3); -1 = placeholder
c.banner = 129;
int index = variants::AddCharacter(c); // strings are copied; returns the entry's index
```

The new entry goes after the built-in 27, on page 2 and later pages as the list grows.

Hitbox rules (`ActivateVariant`): the record's hitbox, raised to at least Freia's height (18) except for Ubiquitous
and Kei. Ai and Mikan use Freia's exactly. Add special cases there if a new character needs one.

### 3. Portrait and banner (optional)

`tools\charsel\make_portraits.py` packs every picture pdnet adds into `kit\pdnet\charsel-portraits.bin`. pdnet loads it
into the game's `menu\MNet.alr` at start (`MakeMnetCopy`).

1. Put the portrait in `tools\charsel\portraits\<name>.png`: 134 x 96 RGBA, background cut out, untinted. The tool gives
   it the players' blue-green.
2. Add `'<name>': (x, y, width)` to `ADDED` in make_portraits.py. That is the banner's box in the portrait (around the
   face, height = width x 24 / 74); the tool cuts it and colours it like the game's red banners.
3. Run `py -I tools\charsel\make_portraits.py --pack` (needs numpy and Pillow).
4. Read `tools\charsel\sprites.txt` for the new `portrait_<name>.tga` and `banner_<name>.tga` numbers. Put them in the
   entry's `portrait` / `banner`.
5. Ship the new `charsel-portraits.bin` with the dll (it sits in the kit's root, next to pdnet.dll).

Room: 25 more portraits, 33 more banners, 186 sprites in all (MakeMnetCopy grows MNet.alr's metadata to 0x3000; more
would need another page there and more art space, see make_portraits.py). Without pictures, the grid shows the
placeholder (Alpha's portrait) and the lobby shows the carrier's monster.

### 4. More movement types or animation sets (optional)

Add to `kDodges` / `kSets` in variants.cpp. A dodge is a battle folder whose motions 33-39 are used, plus a movement
skill (750 jump, 751 cartwheel / roll). A set is a battle folder for all other motions and the exmots, plus the
charaset record whose movement stats (turning, speeds) go with it. The pop-ups grow by themselves (up to 8 rows), and
names are shared online, so every PC needs the same lists.

## Testing

- Offline: Lab > Test Arsenal, Y on the character select, pick the new entry. `variant_test=<character>,<colour>,<dodge>,<set>`
  in pdnet.ini (list numbers) plays it without the menu.
- pdnet.log: every line starts with `variants:`. Look for `Advanced list:` (the pick), `slot N hitbox`, `prebuild ...
  done: 7 of 7 files`, `build FAILED` (with the reason), `share:` and `online:` (lobbies), `pick table (...)`.
- `menu_debug=on` logs the menu scripts' commands on the character select.

## Things that should stay as they are

- The carriers (`kCarriers`) and the lobby slot order: every PC must agree on them.
- The MNet.alr sprite layout (sprites 84 on come from charsel-portraits.bin in its order; the code finds the built-in
  ones by name, and added ones by the numbers in their entries).
- Entry names of characters already released: they are what other PCs look up.
