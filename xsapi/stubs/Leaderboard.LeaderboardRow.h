#pragma once
#include "Leaderboard/LeaderboardRow.g.h"

// pdnet-hand: one scoreboard row from an EOS leaderboard (see src/scores.h): the board's stat, then extra columns.
namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardRow : LeaderboardRowT<LeaderboardRow>
    {
        LeaderboardRow() = default;
        LeaderboardRow(hstring gamertag, hstring xuid, uint32_t rank, double percentile, std::vector<hstring> values)
            : m_gamertag(std::move(gamertag)), m_xuid(std::move(xuid)), m_rank(rank), m_percentile(percentile), m_values(std::move(values)) {}

        hstring Gamertag() { return m_gamertag; }
        hstring XboxUserId() { return m_xuid; }
        double Percentile() { return m_percentile; }
        uint32_t Rank() { return m_rank; }
        Windows::Foundation::Collections::IVectorView<hstring> Values() { return winrt::single_threaded_vector<hstring>(std::vector<hstring>(m_values)).GetView(); }

    private:
        hstring m_gamertag, m_xuid;
        uint32_t m_rank = 0;
        double m_percentile = 0;
        std::vector<hstring> m_values;
    };
}
