#pragma once
#include "Leaderboard/LeaderboardColumn.g.h"

// pdnet-hand: the scoreboard's one column, the multiplayer rating.
namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardColumn : LeaderboardColumnT<LeaderboardColumn>
    {
        LeaderboardColumn() = default;

        hstring DisplayName() { return L"Rating"; }
        hstring StatisticName() { return L"Hero_MP_Ranking"; }
        Windows::Foundation::PropertyType StatisticType() { return Windows::Foundation::PropertyType::Int64; }
    };
}
