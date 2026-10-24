#pragma once
#include "Leaderboard/LeaderboardRow.g.h"

// pdnet-hand: one scoreboard row from the EOS leaderboard (see src/scores.h).
namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardRow : LeaderboardRowT<LeaderboardRow>
    {
        LeaderboardRow() = default;
        LeaderboardRow(hstring gamertag, hstring xuid, uint32_t rank, double percentile, hstring value)
            : m_gamertag(std::move(gamertag)), m_xuid(std::move(xuid)), m_rank(rank), m_percentile(percentile), m_value(std::move(value)) {}

        hstring Gamertag() { return m_gamertag; }
        hstring XboxUserId() { return m_xuid; }
        double Percentile() { return m_percentile; }
        uint32_t Rank() { return m_rank; }
        Windows::Foundation::Collections::IVectorView<hstring> Values() { return winrt::single_threaded_vector<hstring>({ m_value }).GetView(); }

    private:
        hstring m_gamertag, m_xuid;
        uint32_t m_rank = 0;
        double m_percentile = 0;
        hstring m_value;
    };
}
