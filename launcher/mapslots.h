#pragma once
// New map slots (Conceptual; docs\CONCEPTUAL.md "New map slots"): map mods (a .alr + .dat pair, e.g. Nuion's format with
// mod-info.ini and a 254x112 preview.png) become new stages in the multiplayer map picker. pdlaunch prebakes them at
// every game start, before PDPL loads, as loose files in PDPL's mods folder (they win over every archive):
//   Assets/Data/setup/stageset/stageset_en.dat, _ja.dat  the game's 30 stages + one entry per map mod (entry index = map
//                                                       number, 30-64 skipping numbers ending in 1 or 9: 28 slots), blank
//                                                       filler entries for the skipped numbers
//   Assets/Data/Map/st<NN>.alr, .dat                     each map mod's files under its slot's number
//   Assets/Data/Menu/MNet.alr                            the game's, with the previews on new 256x1024 DXT1 sheets
//                                                       (textures 12+, 9 strips each) and sprites 84+ for them
// and LocalState\pdnet\map-slots.txt for pdnet (map picker items, unlocked answer, preview sprite). Built so that pdnet
// can run the same generator live once PDPL can mount while the game runs.
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

    // The slot numbers in order (30, 32-38, 40, ...): 28 of them.
    const std::vector<int>& SlotNumbers();

    // Writes (or removes, with no maps) the generated files. gameDir: the game folder (holds Assets\Data); pdplDir:
    // RoamingState\mods; pdnetDir: LocalState\pdnet. Maps beyond the 28 slots are left out (logged).
    void Generate(const std::wstring& gameDir, const std::wstring& pdplDir, const std::wstring& pdnetDir,
                  const std::vector<MapMod>& maps, LogFn log);
}
