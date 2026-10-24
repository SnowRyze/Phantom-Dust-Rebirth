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
        bool scoreboard = true;             // scoreboard=on: the Scoreboard shows one global EOS leaderboard of the multiplayer rating (see scores.h)
        std::string scoreboardStat = "RATING";          // scoreboard_stat=: EOS stat that holds each player's rating (aggregation Latest)
        std::string scoreboardLeaderboard = "GLOBAL_RATING"; // scoreboard_leaderboard=: EOS leaderboard id built on that stat
        bool shopMaxOut = true;             // shop_max_out=on: the shop's Online Store item gives max credits and skills (see gamepatches.cpp)
        bool lobbyGamertagOption = true;    // lobby_gamertag_option=on: the lobby's Gamertag option works even when alone (see gamepatches.cpp)
        bool voiceChat = true;              // voice_chat=on: lobbies get an EOS voice room; the lobby's Mic option mutes only your mic (see lobbies.h)
        bool voiceListen = true;            // voice_listen=on: hear the other players in the voice room (voice_mode=mic only)
        bool voiceClassic = false;          // voice_mode=classic: the game's Voice Chat option switches talking AND hearing (original game);
                                            // voice_mode=mic (default): it is labelled Mic and switches only this PC's microphone
        std::string voiceTalkMark = "\xE2\x99\xAA"; // voice_talk_mark=: blinks after a talking player's name in the lobby (UTF-8 text, or #N = the game's inline icon N); default a music note
        bool p2pDebug = false;              // p2p_debug=on: log every pdnet control message sent and received over EOS P2P (debug, see p2p.cpp)
        bool voiceDebug = false;           // voice_debug=on: EOS voice logs at Info, the input devices EOS sees, and the mic level every 3 s (debug)
        int voiceTalkIcon = 36;             // voice_talk_icon=N: MNET sprite N after the name of each player whose mic is on (36 = the original game's megaphone); -1 = voice_talk_mark text after a talking player's name instead
        int voiceTalkIconX = 6;             // voice_talk_icon_x=: gap in pixels between the end of the name and the megaphone
        bool iconGallery = false;         // icon_gallery=on: the main menu's hint line shows the game's 37 inline icons (debug, see gamepatches.cpp)
        bool nametagsOnBodies = true;    // nametags_on_bodies=on: keep nametags over dead players (game code patch, see gamepatches.h)
        uint32_t deadNametagColor = 0xFFFFE600; // dead_nametag_color=RRGGBB: colour of a dead player's nametag (default yellow; Tag Team only)
        bool nametagColors = true;              // nametag_colors=on|off: off = every nametag in the game's own white (dead bodies too)
        uint32_t allyNametagColor = 0xFF4FB4FF; // ally_nametag_color=RRGGBB: a teammate's nametag in Tag Team (default blue)
        uint32_t enemyNametagColor = 0xFFFF5A5A; // enemy_nametag_color=RRGGBB: an opponent's nametag, every game type (default red)
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
