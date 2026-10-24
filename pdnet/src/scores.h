#pragma once
// Scoreboard through EOS (scoreboard=on): global leaderboards shared by every player on the EOS deployment, the
// same four the original Xbox scoreboard had. Each EOS leaderboard id and the stat it ranks are named as the game
// names them:
//   CurrentMPRanking  the multiplayer rating (stat aggregation Latest); its rows also show TotalWin
//   BattleRoyaleWin, TagTeamWin, OneOnOneWin   wins per mode (aggregation Sum)
//   (Total Wins, the Rating board's column, is those three added up)
//   PlayerName0-8     the player's display_name, packed 31 bits per stat (aggregation Latest; EOS keeps no names
//                     for device-id accounts)
// The rating comes from the game's Hero_MP_Ranking event (and once per session from the save); a win event adds 1
// to its mode and to TotalWin. All of it is set up in the EOS Developer Portal (HANDOFF, "Scoreboard").
// Plain C++ for the XSAPI stand-in.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace pdnet::scores
{
    struct Record
    {
        uint32_t rank = 0;   // 1 = top
        int32_t score = 0;   // the leaderboard's stat
        std::string userId;  // EOS product user id
        std::string name;    // UTF-8 display name (the player's pdnet name)
        bool me = false;     // this PC's player
    };

    // Sends the rating to CurrentMPRanking (asynchronous; skipped while EOS is signed out or when unchanged).
    void SubmitRating(int rating);

    // A multiplayer win: `mode` is BattleRoyaleWin, TagTeamWin or OneOnOneWin. Adds 1 to it.
    void AddWin(const std::string& mode);

    // A leaderboard's ranks (EOS keeps the top 1000), refreshed at most every 20 s. Waits on the calling thread
    // (up to ~12 s). Empty if EOS is signed out or the leaderboard is missing.
    std::vector<Record> Ranks(const std::string& leaderboard);

    // The game session this PC is in: 1 = Rated, 0 = Unrated (its Rating option: search handle ratinglimit),
    // -1 = no session with that setting found. Implemented in xsapi/mpsd.cpp.
    int RatedMatch();

    // The Rated setting of the sessions the last search returned, in the order the game lists them (1 / 0 / -1).
    // Implemented in xsapi/mpsd.cpp; set by the search (Multiplayer.MultiplayerService.cpp).
    void SetSearchResultsRated(std::vector<int> rated);
    int SearchResultRated(size_t index);

    // Players' names (UTF-8) by EOS user id, from the PlayerName0-8 stats (each player's display_name, sent once per
    // session when the scoreboard opens; aggregation Latest).
    std::map<std::string, std::string> DisplayNames(const std::vector<std::string>& userIds);

    // Each player's total of a Sum stat (for extra columns such as TotalWin); missing players are left out.
    std::map<std::string, int32_t> UserScores(const std::string& stat, const std::vector<std::string>& userIds);
}
