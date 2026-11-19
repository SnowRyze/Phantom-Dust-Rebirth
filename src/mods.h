#pragma once
// Mods and lobbies (Conceptual, mod loader step 7; docs\MOD_LOADER_CODE.md). Plain C++ like lobbies.h, so the XSAPI
// stand-in can use it. Everything comes from LocalState\pdnet\mods-state.txt, which pdlaunch writes at every game start
// (launcher\modsync.cpp): the mods PDPL loaded this start. They can't change until the next start (PDPL 1.2.2 has no way
// to swap mods while the game runs), so a lobby whose mods differ is set up for the next start instead.
#include <string>
#include <vector>

namespace pdnet::mods
{
    // This start's gameplay mods, as a lobby carries them (attribute PD_MODS): every mod that's on, loaded by PDPL, and
    // not "online": "local" (those only change how things look or sound on this PC), in priority order, as
    // "<id>:<first 12 hex of the archive's sha256>[:<release tag>]" (the tag for catalog mods, so a joiner can download
    // the same release), comma-separated. Empty: no gameplay mods (or PDPL isn't in the game).
    const std::string& LobbyTag();
    inline bool InUse() { return !LobbyTag().empty(); }

    // A lobby whose PD_MODS isn't this start's: true when they differ (an empty tag is a lobby without mods).
    inline bool Differs(const std::string& lobbyTag) { return lobbyTag != LobbyTag(); }

    // How many mods a lobby's PD_MODS names.
    size_t Count(const std::string& lobbyTag);

    // Joining a lobby whose mods differ: writes LocalState\pdnet\lobby-mods.txt, so pdlaunch uses exactly the lobby's
    // mods at the next game start (catalog ones downloaded at the lobby's release; this PC's own "online": "local" mods
    // stay). False with the reason when lobby_mods=off.
    bool RequestForNextStart(const std::string& lobbyId, const std::string& lobbyTag, std::string& why);

    // This start uses a lobby's mods (pdlaunch read lobby-mods.txt).
    bool LobbyStart();

    // The Mods list replaces the multiplayer menu's Quick Join, but that item's script still starts Quick Join's lobby
    // search as it opens the list (the game would join the first result). gamepatches calls ListOpened when it turns the
    // pick into the Mods list; a Quick Join search within 3 s of that answers no lobbies (MultiplayerService).
    void ListOpened();
    bool ListJustOpened();

    // The Lobby Browser's search results, in order (MultiplayerService), for SESSION INFO (gamepatches.cpp).
    void SetSearchResults(std::vector<std::string> tags);
    std::string SearchResultTag(size_t index);
}
