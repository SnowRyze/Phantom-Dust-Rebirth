#pragma once
// Conceptual branch only: Theatre films, step 1 (the recorder). See film.cpp and docs\CONCEPTUAL.md.
#include <cstdint>
#include <string>

namespace pdnet::film
{
    // Starts the watcher that opens a film when a match starts and closes it when the match ends.
    void Start();

    // A game datagram passing through pdnet (p2p.cpp): `out` true for one this PC's game sent to `peer`, false for one
    // `peer` sent this PC's game. Cheap when no film is being recorded; never waits on the disk.
    void Packet(bool out, const std::string& peer, const uint8_t* data, size_t bytes);
}
