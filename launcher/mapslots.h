#pragma once
// New map slots (Conceptual; docs\CONCEPTUAL.md "New map slots"): map mods (a .alr + .dat pair, e.g. Nuion's format with
// mod-info.ini and a 254x112 preview.png) become new stages in the multiplayer map picker. pdlaunch prebakes them at
// every game start, before PDPL loads, as loose files in PDPL's mods folder (they win over every archive):
//   Assets/Data/setup/stageset/stageset_en.dat, .dat   the game's 30 stages + one entry per map mod (entry index = map
//                                                       number, 30-64 skipping numbers ending in 1 or 9 and 32: 27 slots),
//                                                       blank filler entries for the skipped numbers
//   Assets/Data/Map/st<NN>.alr, .dat                     each map mod's files under its slot's number
// and in LocalState\pdnet: map-slots.txt (map picker rows, unlocked answer, map load names) and map-previews.bin (the
// previews on one DXT1 sheet, which pdnet adds to its MNet.alr copy with the character select's portraits). Built so
// that pdnet can run the same generator live once PDPL can mount while the game runs.
#include <string>
#include <vector>

namespace mapslots
{
    using LogFn = void (*)(const wchar_t* fmt, ...);

    struct MapMod
    {
        std::string id, name;         // the mod's id and display name
        std::string alr, dat;         // the map's files (bytes)
        std::string preview;          // preview image bytes (PNG or anything WIC reads), may be empty
    };

    // The slot numbers in order (30, 33-38, 40, ...): 27 of them.
    const std::vector<int>& SlotNumbers();

    // Writes (or removes, with no maps) the generated files. gameDir: the game folder (holds Assets\Data); pdplDir:
    // RoamingState\mods; pdnetDir: LocalState\pdnet. Maps beyond the 27 slots are left out (logged).
    void Generate(const std::wstring& gameDir, const std::wstring& pdplDir, const std::wstring& pdnetDir,
                  const std::vector<MapMod>& maps, LogFn log);
}
