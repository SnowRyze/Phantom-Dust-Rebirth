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
        std::string players;  // "SnowRyze, Motley"
        double minutes = 0;
        uint32_t packets = 0;
    };
    std::vector<Info> List(size_t max);
}
