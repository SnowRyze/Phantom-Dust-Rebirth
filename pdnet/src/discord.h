#pragma once
// Discord channel announcements through a webhook (discord_webhook= in pdnet.ini): the PC that owns a public lobby
// posts one message when it opens and edits that message as players join and leave and when it closes (the message id
// travels in the lobby as PD_DISCORD, so a new owner keeps editing it). Plain C++ like lobbies.h, so the XSAPI
// stand-in can call it. Every announcement is posted on a worker thread and never blocks the game.
#include "lobbies.h"
#include <string>

namespace pdnet::discord
{
    // POSTs {"content": text} to `url` now (blocking). Mentions are disabled, so names like "@everyone" don't ping anyone.
    bool PostNow(const std::wstring& url, const std::string& text);

    // A player name for a message: Discord markdown escaped, "Someone" if empty.
    std::string Name(const std::string& s);

    // A session property from the lobby (PD_PROPS holds name -> raw JSON text); quotes around strings are removed.
    std::string LobbyProperty(const lobbies::Lobby& l, const wchar_t* name);

    // "Tag Team · Standard Battle": the game type and rule from the lobby's search handle (empty if it has none yet).
    std::string MatchDescription(const lobbies::Lobby& l);

    // Lobby announcements (no-ops without discord_webhook, and for lobbies other than the game's public SearchableGame).
    void LobbyOpened(const lobbies::Lobby& l);   // this PC created it ("opened" is posted once it has the game's search handle)
    void LobbyChanged(const lobbies::Lobby& l);  // live copy after any lobby event or search handle change
    void LobbyLeft(const std::string& id, bool lastMember); // this PC left (lastMember: the lobby was destroyed)
    void LobbyLost(const std::string& id);       // EOS closed the lobby or removed this PC
    void LobbyGone(const lobbies::Lobby& l);       // the game host left a lobby others stayed in: its message says closed
}
