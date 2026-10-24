#pragma once
// Scoreboard through EOS (scoreboard=on): one global leaderboard for every player on the EOS deployment, ranked by
// the game's own multiplayer rating. The rating goes to an EOS stat (aggregation Latest) whenever the game reports
// it (its "Hero_MP_Ranking" event) and once per session; the scoreboard reads the leaderboard built on that stat.
// Both are set up in the EOS Developer Portal (see HANDOFF, "Scoreboard"). Plain C++ for the XSAPI stand-in.
#include <cstdint>
#include <string>
#include <vector>

namespace pdnet::scores
{
    struct Record
    {
        uint32_t rank = 0;   // 1 = top
        int32_t score = 0;   // rating
        std::string userId;  // EOS product user id
        std::string name;    // UTF-8 display name (the player's pdnet name)
        bool me = false;     // this PC's player
    };

    // Sends `rating` to the EOS stat (asynchronous; skipped while EOS is signed out).
    void SubmitRating(int rating);

    // The leaderboard's ranks (EOS keeps the top 1000), refreshed at most every 20 s. Waits on the calling thread
    // (up to ~12 s). Empty if EOS is signed out or the leaderboard is missing.
    std::vector<Record> Ranks();
}
