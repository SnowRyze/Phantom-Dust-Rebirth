#pragma once
#include <map>
#include <string>
#include <vector>
// Small in-memory changes to the game's own code (PDUWP.exe), applied once at startup. Each one checks that the bytes
// it replaces are exactly the expected ones first, so a different game version is left alone. No files are changed.
namespace pdnet::gamepatches
{
    void Apply();

    // Sends a call in the game's code at `site` (a 5-byte call or jump to `target`, both Ghidra addresses) to `hook` instead.
    // False, with nothing changed, if the code there isn't that call.
    bool HookCall(uint64_t site, uint64_t target, void* hook);

    // "Rebirth" on the title screen: the game's file opens are hooked (CreateFile2). The title picture loads
    // seconds before Apply runs, so pdlaunch's watcher calls this until it returns true.
    bool HookFileOpens();
    // The B button for the start-up logos, movies and hung match loads: installed as soon as pdnet is in the game, as the logos
    // are over before Apply runs.
    void InstallEarlyHooks();

    // Nametags on dead bodies can be switched at any time: in a lobby the game host's setting applies to everyone
    // (xsapi/mpsd.cpp reads it from the host's lobby member attributes), otherwise this PC's nametags_on_bodies.
    void SetNametagsOnBodies(bool on, const wchar_t* why);
    void UseOwnSettings();

    // Lobby Custom Settings (the lobby Options menu's Gamertag row): starting health, starting Aura
    // and an Aura refill multiplier (x10) that the lobby host sets for everyone. xsapi/mpsd.cpp publishes this PC's as
    // the member attribute PD_MATCH ("health,aura,refill") and hands every PC the host's (SetHostMatchSettings; `mine`:
    // this PC hosts, or there is no host value). Any value off its default makes the match Unrated.
    struct MatchSettings
    {
        int health = 20, aura = 0, refill = 10;
        bool Custom() const { return health != 20 || aura != 0 || refill != 10; }
    };
    std::string MatchSettingsAttribute(); // this PC's own, as PD_MATCH
    void SetHostMatchSettings(const std::string& attribute, bool mine);
    MatchSettings MatchSettingsInUse();
    // A battle has started (labtests' camera hook): the Aura refill multiplier goes on every player's rate.
    void MatchStarted();

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
    // True while this PC's player has Mind Reading ("listen in on target team's conversation"): the team channels
    // then let the other team be heard too, as the game's own voice rule did.
    bool MindReading();
    // True from Lab > Test Arsenal until back in the menus: its wins don't count for the scoreboard.
    bool TestBattle();
    // Theatre films: slot's Advanced pick (the PD_VARIANT text, "1|slot|character|colour|dodge|set") while that slot plays
    // a variant, into `out`; false (and "") for a normal pick.
    bool FilmVariant(int slot, char* out, size_t size);

    // The multiplayer rating as kept in the save (progress.dat; 0x1405b1370), or -1 before the save is loaded.
    int SavedRating();

    // The game reported a win in the current match (its win event): its rating result is at least +1.
    void NoteMatchWin();

    // The lobby this player is in closed, removed them, or lost its game host (xsapi/mpsd.cpp). If they are in the
    // Arsenal editor opened from the lobby, leaving it takes them out the way the lobby would, not back into it.
    void LobbyEnded();

    // Advanced list variants online: this PC's lobby member attribute PD_VARIANT (empty: a normal pick), and every
    // update of a game lobby's members (EOS user id, PD_NAME, PD_VARIANT) / leaving it. xsapi/mpsd.cpp calls these.
    struct LobbyVariant { std::string userId, name, value; };
    std::string VariantAttribute();
    void LobbyVariants(const std::string& lobbyId, const std::vector<LobbyVariant>& members);
    void LobbyVariantsLeft(const std::string& lobbyId);

    // True while the save's multiplayer profile is the active one (online play; the story uses the other).
    bool MultiplayerProfile();
}
