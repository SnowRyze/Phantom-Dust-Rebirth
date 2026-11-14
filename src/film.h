#pragma once
// Conceptual branch only: Theatre films, step 1 (the recorder). See film.cpp and docs\CONCEPTUAL.md.
#include <cstdint>
#include <string>
#include <vector>

namespace pdnet::film
{
    // Starts the watcher that opens a film when a match starts and closes it when the match ends.
    void Start();

    // A game datagram passing through pdnet (p2p.cpp): `out` true for one this PC's game sent to `peer`, false for one
    // `peer` sent this PC's game. Cheap when no film is being recorded; never waits on the disk.
    void Packet(bool out, const std::string& peer, const uint8_t* data, size_t bytes);

    // The saved films, newest first (at most `max`), as the Theatre list shows them. Unreadable files are left out.
    struct Info
    {
        std::wstring file;    // name in LocalState\pdnet\films
        std::string when;     // "Nov 13 01:45" (from the file name)
        int gameType = -1;    // 0 Battle Royale, 1 Tag Team, 2 One-on-One (-1: no setup recorded)
        int stage = -1;
        int layout = 0;
        uint32_t version = 0;   // its setup snapshot: 1, or 2 with characters and every player's arsenal
        bool present[4]{};
        int character[4]{ -1, -1, -1, -1 };
        std::string players;  // "SnowRyze, Motley"
        double minutes = 0;
        uint32_t packets = 0;
        // Each film slot's deck (30 skill ids, -1 unused): recorded in version 2; otherwise Load rebuilds it from the skills
        // seen in that player's hand (CardKind), so the battle loads every skill the film uses.
        int16_t skills[4][30];
        bool haveSkills[4]{};
        std::string name[4];  // each slot's player name, when recorded
        Info() { for (auto& deck : skills) for (int16_t& id : deck) id = -1; }
    };
    std::vector<Info> List(size_t max);

    // One stream record from a film's match traffic: who it is about (the sender's slot), which of the game's 26 streams,
    // when (ms since the film started), the sender's game frame / 2, its sequence number (sequenced streams) and data.
    struct Event
    {
        uint32_t ms = 0, half = 0, number = 0;
        uint8_t slot = 0, stream = 0;
        std::vector<uint8_t> data;
    };
    // Reads and decodes a film (bounds-checked): its stream records in time order, and its details. False if unreadable.
    bool Load(const std::wstring& file, std::vector<Event>& events, Info& info);
}
