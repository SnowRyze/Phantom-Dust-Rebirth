#pragma once
// Mods and lobbies (Conceptual, mod loader step 7; docs\MOD_LOADER_CODE.md). Plain C++ like lobbies.h, so the XSAPI
// stand-in can use it. Everything comes from LocalState\pdnet\mods-state.txt, which pdlaunch writes at every game start
// (launcher\modsync.cpp): the mods PDPL has loaded. They change at game start, or live (ApplyLive) when nothing that loads
// only at start is involved; a lobby whose mods differ and can't be loaded now is set up for the next start instead.
#include <windows.h>
#include <string>
#include <vector>

namespace pdnet::mods
{
    // This start's gameplay mods, as a lobby carries them (attribute PD_MODS): every mod that's on, loaded by PDPL, and
    // not "online": "local" (those only change how things look or sound on this PC), in priority order, as
    // "<id>:<first 12 hex of the archive's sha256>[:<release tag>]" (the tag for catalog mods, so a joiner can download
    // the same release), comma-separated. Empty: no gameplay mods (or PDPL isn't in the game).
    std::string LobbyTag();
    inline bool InUse() { return !LobbyTag().empty(); }

    // A lobby whose PD_MODS isn't this start's: true when they name different mods (an empty tag is a lobby without
    // mods). The order doesn't count (the user, 2026-11-22: the same two map mods in another order refused the join).
    bool Differs(const std::string& lobbyTag);

    // How many mods a lobby's PD_MODS names.
    size_t Count(const std::string& lobbyTag);

    // Joining a lobby whose mods differ: writes LocalState\pdnet\lobby-mods.txt, so pdlaunch uses exactly the lobby's
    // mods at the next game start (catalog ones downloaded at the lobby's release; this PC's own "online": "local" mods
    // stay). False with the reason when lobby_mods=off.
    bool RequestForNextStart(const std::string& lobbyId, const std::string& lobbyTag, std::string& why);

    // Live mod loading: pdlaunch syncs the mods again (settings.ini's list, or lobby-mods.txt's) and PDPL's archives are
    // swapped, all while the game runs; then the lobby tag and the map slots are read again. All or nothing: Restart when
    // something changes that the game or PDPL only reads at start (`why` names it; nothing was changed, it's done at the
    // next start), Failed / NotNow when it couldn't be done (a match, PDPL without exports, an older pdlaunch). Waits up to
    // timeoutMs for the result; past that it goes on in the background (Busy). Not on the game's main thread.
    enum class Live { Done, Restart, NotNow, Failed, Busy };
    bool LiveAvailable();
    Live ApplyLive(const char* reason, DWORD timeoutMs, std::string& why);

    // The mods in use are a lobby's (pdlaunch read lobby-mods.txt).
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
