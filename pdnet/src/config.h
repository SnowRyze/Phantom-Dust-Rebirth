#pragma once
#include <string>
#include <cstdint>
#include <vector>

namespace pdnet
{
    // Fixed (not read from pdnet.ini, 2026-10-24): networking (EOS P2P, no Xbox Live, ports, relay), test_peer, eos_test,
    // and the features lobby_arsenal_editor, lobby_gamertag_option, shop_max_out, rated_lobbies (each lobby picks Rated /
    // Unrated in the game), scoreboard, the megaphone voice icon and remember_volume (pdlaunch).
    struct Config
    {
        bool teredo = false;                // network_mode=teredo: leave Xbox networking (Teredo/SDA) to Windows
        bool p2p = true;                    // match traffic through EOS P2P (NAT traversal + relays, see p2p.h)
        std::wstring p2pRelay = L"allow";   // p2p_relay=allow|force|off: EOS relay use (force = always relay, for testing)
        std::wstring advertiseIp = L"auto"; // address peers use to reach this PC, or "auto" (primary IPv4)
        uint16_t gamePort = 43210;          // must match networkmanifest.xml (PeerSocketUDP)
        uint16_t controlPort = 43211;       // pdnet control channel (connect handshake, pings)
        uint32_t qosProbes = 5;
        bool testPeer = false;              // test_peer=on: in-process fake second player (debug, see testpeer.h)
        bool menuDebug = false;             // menu_debug=on: log the lobby script's commands (debug, see gamepatches.cpp)
        bool lobbyArsenalEditor = true;     // lobby_arsenal_editor=on: the lobby's invite button opens the Arsenal editor (see gamepatches.h)
        bool ratedLobbies = true;           // rated_lobbies=on: the lobby's Rating option is Unrated / Rated; only Rated matches change ratings (see gamepatches.cpp)
        bool scoreboard = true;             // scoreboard=on: the Scoreboard shows global EOS leaderboards (rating, wins per mode) (see scores.h)
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
        uint32_t allyNametagColor = 0xFF4FB4FF; // ally_nametag_color=RRGGBB: a teammate's nametag in Tag Team (default blue)
        uint32_t enemyNametagColor = 0xFFFF5A5A; // enemy_nametag_color=RRGGBB: an opponent's nametag, every game type (default red)
        int nametagDebug = 0;               // nametag_debug=on (1): log the nametag-related fields of each player when they change; deep (2): also rare changes in player 1's game structures (debug, see gamepatches.cpp)
        int nameTrace = 0;                  // name_trace=draw|on (2): log game text draws of other players' names; memory (1): log reads of name copies (debug, see xsapi/nametrace.h)
        bool eosTest = false;               // eos_test=on: one-shot Epic Online Services feasibility check (see eostest.h)
        bool eos = true;                    // stay signed in to Epic Online Services (see eos.h)
        bool xboxOff = true;                // pdnet answers every Xbox Live (XSAPI) class itself (see xsapi/)
        std::wstring displayName = L"Player"; // display_name=: name shown to other players (max 32 characters)
        std::wstring discordWebhook;        // discord_webhook=<URL>: announce lobbies this PC hosts in a Discord channel (see discord.h)
        bool discordLobbyMessages = true;   // discord_lobby_messages=on|off: "opened" / "closed" messages
        bool discordPlayerMessages = true;  // discord_player_messages=on|off: "<name> joined / left" (and "full") messages
        std::wstring logWebhook;            // log_webhook=<URL>: where Options > Send Log uploads this PC's logs (see discord.h)
        std::wstring discordInvite;         // discord_invite=<URL>: the community Discord the main menu's Community button opens (see gamesave.cpp)
        int splashVolume = 40;              // splash_volume=: % of the remembered volume from the start-up logos until the main menu (pdlaunch; the Settings menu shows it)
        bool auraBeeps = true;              // aura_beeps=on|off: the aura particles' beeping (Options > Sound shows it)
        bool moreControllers = true;        // more_controllers=on|off: PlayStation, Nintendo and other controllers, read by pdlaunch with SDL3 (see gamepads.cpp)
        bool soundLog = false;              // sound_log=on: log each game sound (bank, cue, caller) the first time it plays (debug, see gamepatches.cpp)
    };

    // Loaded once: pdnet.ini next to the DLL, else LocalState\pdnet.ini (written with defaults if missing).
    const Config& GetConfig();

    // The same settings, for the in-game Settings menu (gamepatches.cpp): change fields, then SaveSettings writes
    // key=value lines to LocalState\pdnet\settings.ini (read after pdnet.ini, so they win; the game can't write the kit
    // folder). pdlaunch reads splash_volume from there too.
    Config& EditableConfig();
    void SaveSettings(const std::vector<std::pair<std::string, std::string>>& values);
    void SaveSetting(const std::string& key, const std::string& value);
}
