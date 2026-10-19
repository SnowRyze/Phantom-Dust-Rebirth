#pragma once
#include <string>
#include <cstdint>

namespace pdnet
{
    struct Config
    {
        bool teredo = false;                // network_mode=teredo: leave Xbox networking (Teredo/SDA) to Windows
        std::wstring advertiseIp = L"auto"; // address peers use to reach this PC, or "auto" (primary IPv4)
        uint16_t gamePort = 43210;          // must match networkmanifest.xml (PeerSocketUDP)
        uint16_t controlPort = 43211;       // pdnet control channel (connect handshake, pings)
        uint32_t qosProbes = 5;
        bool testPeer = false;              // test_peer=on: in-process fake second player (debug, see testpeer.h)
        bool eosTest = false;               // eos_test=on: one-shot Epic Online Services feasibility check (see eostest.h)
    };

    // Loaded once: pdnet.ini next to the DLL, else LocalState\pdnet.ini (written with defaults if missing).
    const Config& GetConfig();
}
