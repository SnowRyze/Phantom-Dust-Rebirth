#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace pdnet
{
    struct Config
    {
        bool teredo = false;                // network_mode=teredo: leave Xbox networking (Teredo/SDA) to Windows
        std::wstring advertiseIp = L"auto"; // extra fixed public IPv4/IPv6 to offer peers, or "auto"
        uint16_t gamePort = 43210;          // must match networkmanifest.xml (PeerSocketUDP)
        uint32_t qosProbes = 5;
        bool stun = true;                   // ask public STUN servers for this PC's public address
        std::vector<std::wstring> stunServers = { L"stun.l.google.com:19302", L"stun.cloudflare.com:3478" };
        bool upnp = true;                   // ask the router (UPnP) to forward the game port
    };

    // Loaded once: pdnet.ini next to the DLL, else LocalState\pdnet.ini (written with defaults if missing).
    const Config& GetConfig();
}
