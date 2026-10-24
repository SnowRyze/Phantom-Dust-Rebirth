#pragma once
#include "Leaderboard/LeaderboardColumn.g.h"

// pdnet-hand: one scoreboard column: a stat (CurrentMPRanking, TotalWin, BattleRoyaleWin, ...).
namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardColumn : LeaderboardColumnT<LeaderboardColumn>
    {
        LeaderboardColumn() = default;
        explicit LeaderboardColumn(hstring stat) : m_stat(std::move(stat)) {}

        hstring DisplayName() { return m_stat; }
        hstring StatisticName() { return m_stat; }
        Windows::Foundation::PropertyType StatisticType() { return Windows::Foundation::PropertyType::Int64; }

    private:
        hstring m_stat;
    };
}
