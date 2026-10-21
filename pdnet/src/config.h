#pragma once
#include <string>
#include <cstdint>

namespace pdnet
{
    struct Config
    {
        bool teredo = false;                // network_mode=teredo: leave Xbox networking (Teredo/SDA) to Windows
        bool p2p = false;                   // network_mode=eos: match traffic through EOS P2P (NAT traversal + relays, see p2p.h)
        std::wstring p2pRelay = L"allow";   // p2p_relay=allow|force|off: EOS relay use (force = always relay, for testing)
        std::wstring advertiseIp = L"auto"; // address peers use to reach this PC, or "auto" (primary IPv4)
        uint16_t gamePort = 43210;          // must match networkmanifest.xml (PeerSocketUDP)
        uint16_t controlPort = 43211;       // pdnet control channel (connect handshake, pings)
        uint32_t qosProbes = 5;
        bool testPeer = false;              // test_peer=on: in-process fake second player (debug, see testpeer.h)
        bool menuDebug = false;             // menu_debug=on: log the lobby script's commands (debug, see gamepatches.cpp)
        bool lobbyArsenalEditor = true;     // lobby_arsenal_editor=on: the lobby's invite button opens the Arsenal editor (see gamepatches.h)
        bool nametagsOnBodies = true;      // nametags_on_bodies=on: keep nametags over dead players (game code patch, see gamepatches.h)
        uint32_t deadNametagColor = 0xFFFFE600; // dead_nametag_color=RRGGBB: colour of a dead player's nametag (default yellow)
        int nametagDebug = 0;               // nametag_debug=on (1): log the nametag-related fields of each player when they change; deep (2): also rare changes in player 1's game structures (debug, see gamepatches.cpp)
        int nameTrace = 0;                  // name_trace=draw|on (2): log game text draws of other players' names; memory (1): log reads of name copies (debug, see xsapi/nametrace.h)
        bool eosTest = false;               // eos_test=on: one-shot Epic Online Services feasibility check (see eostest.h)
        bool eos = false;                   // eos=on: stay signed in to Epic Online Services (see eos.h)
        bool xboxOff = false;               // xbox=off: pdnet answers every Xbox Live (XSAPI) class itself (see xsapi/)
        std::wstring displayName = L"Player"; // display_name=: name shown to other players (max 32 characters)
        std::wstring discordWebhook;        // discord_webhook=<URL>: announce lobbies this PC hosts in a Discord channel (see discord.h)
        bool discordLobbyMessages = true;   // discord_lobby_messages=on|off: "opened" / "closed" messages
        bool discordPlayerMessages = true;  // discord_player_messages=on|off: "<name> joined / left" (and "full") messages
    };

    // Loaded once: pdnet.ini next to the DLL, else LocalState\pdnet.ini (written with defaults if missing).
    const Config& GetConfig();
}
