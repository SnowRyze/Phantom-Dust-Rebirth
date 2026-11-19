#pragma once
// Theatre films: the recorder and reader (made on the Conceptual branch, notes in its docs\CONCEPTUAL.md). See film.cpp.
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

    // The game lobby (EOS lobby id of the multiplayer session) this PC is in, from xsapi\mpsd.cpp when it joins or
    // creates one. Each film records it, with its lobby session number and its match number in that lobby.
    void SetLobby(const std::string& lobbyId);

    // A player's camera tilt (the up/down look, sine of the angle, -1 down .. 1 up), which the game's own traffic doesn't
    // carry: this PC's own, read by the watcher, and other players' from p2p.cpp (pdnet sends it alongside the game).
    void Look(int slot, float tilt);
    // Film events with this stream number are Look records: data = the tilt as an int16 (tilt * 30000).
    constexpr uint8_t kLookStream = 200;

    // The saved films, newest first (at most `max`), as the Theatre list shows them. Unreadable files are left out.
    struct Info
    {
        std::wstring file;    // name in LocalState\pdnet\films
        std::string when;     // "Nov 13 01:45" (from the file name)
        int gameType = -1;    // the game's DAT_1405277b4: not the match type (1v1s and 2v2s both record 1); see MatchType
        int team[4]{ -1, -1, -1, -1 };
        uint32_t sequence = 0;     // films are numbered in the order they're recorded (0: older films, before numbering)
        uint32_t lobbySession = 0; // which lobby session (1, 2, ... each new lobby joined), 0: not recorded
        uint32_t matchInLobby = 0; // its match number in that lobby
        std::string lobby;         // the lobby's id
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
    // "One-on-One" (2 players), "Tag Team" (more, in 2 teams) or "Battle Royale" (each on their own), from its players.
    const char* MatchType(const Info& info);

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
