#pragma once
#include <map>
#include <string>
// Small in-memory changes to the game's own code (PDUWP.exe), applied once at startup. Each one checks that the bytes
// it replaces are exactly the expected ones first, so a different game version is left alone. No files are changed.
namespace pdnet::gamepatches
{
    void Apply();

    // Nametags on dead bodies can be switched at any time: in a lobby the game host's setting applies to everyone
    // (xsapi/mpsd.cpp reads it from the host's lobby member attributes), otherwise this PC's nametags_on_bodies.
    void SetNametagsOnBodies(bool on, const wchar_t* why);
    void UseOwnSettings();

    // The lobby's invite button (no Xbox Live invites any more) opens the Arsenal editor; leaving it returns to the
    // lobby. Returns false when not in the lobby menu (or the menu hooks aren't installed).
    bool OpenLobbyArsenalEditor();

    // The game's own "Voice Chat" setting (lobby Options, settings flag 15; pdnet labels it "Mic"): false = off.
    // With voice_chat=on it switches only this PC's microphone in the EOS voice room.
    bool GameMicOn();

    // Quick Join: bit N set = this player could play a lobby with game rule N (0-11), i.e. the rule needs no arsenal
    // (Quick) or one of the player's multiplayer arsenals is legal for it - the same test the lobby makes before it
    // sends a player out with "no arsenals for this session". All bits set if the save can't be read.
    uint32_t JoinableRules();

    // Tag Team voice channels: true while a Tag Team match is being played (not in its lobby). Then `teamOf` maps
    // each player's name (as shown in the lobby) to their team, and `myTeam` is this PC's team.
    bool TagTeamMatch(std::map<std::string, int>& teamOf, int& myTeam);
}
