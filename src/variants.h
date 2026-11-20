#pragma once
// Character variants for the character select's Advanced list (docs/CHARACTER_SELECT.md): a character's model with
// the dodge (motions 33-39) of one character and every other motion of another, built when it's picked, never kept
// on disk by the game. The builder here is plain C++ (no Windows calls) so it can be tested off-line.
#include <cstdint>
#include <string>
#include <vector>

namespace pdnet::variants
{
    enum class Kind { Player, Npc, Monster };

    struct Character
    {
        const char* name;       // as the Advanced list shows it
        Kind kind;
        const char* folder;     // Player\<folder> (players, monsters) or the NPC's model name (npcNNa) in Event\*.mk
        int version;            // NPCs with two models (Mac 1 / Mac 2 ...): 0 or 1, by the bundles' order
        int record;             // its charaset record (hitbox; -1: none)
        const char* colours[4]; // colour names; nullptr where it has no such colour file
        const char* voice;      // BattleVo_*.bin
    };

    struct Dodge { const char* name; const char* folder; int move; };      // motions 33-39 from folder; charaset +0x36
    struct AnimationSet { const char* name; const char* folder; int record; }; // every other motion and the exmots

    const std::vector<Character>& Characters();
    const std::vector<Dodge>& Dodges();
    const std::vector<AnimationSet>& AnimationSets();
    int ColourCount(const Character& c);

    struct Pick { int character = -1, colour = 0, dodge = 0, set = 0; };

    // Reads a game file (path relative to Assets\Data, e.g. "Player\\pc02a\\pc02a0.alr"); empty when missing.
    using ReadFn = std::vector<uint8_t> (*)(const std::string& relative);

    // The picked character's battle file: its model (the colour's file), the dodge's motions 33-39 and the set's
    // other motions, each moved onto the model's skeleton. Empty on failure (`error` says why).
    std::vector<uint8_t> BuildModel(const Pick& pick, ReadFn read, std::string& error);
    // exmot0..5.ala (motions 130-135) from the animation set, moved onto the picked character's skeleton.
    std::vector<uint8_t> BuildExmot(const Pick& pick, int k, ReadFn read, std::string& error);

    // Exmot clip lengths of the animation set (charaset +0x40), movement skill of the dodge.
    void SetLengths(const Pick& pick, int16_t lengths[6]);

    // How many joints of the destination skeleton got a track in the last BuildModel (diagnostics).
    struct Stats { int runs = 0, tracks = 0, dropped = 0, joints = 0; };
    Stats LastStats();
}
