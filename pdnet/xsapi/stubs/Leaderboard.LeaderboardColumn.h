#pragma once
#include "Leaderboard/LeaderboardColumn.g.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardColumn : LeaderboardColumnT<LeaderboardColumn>
    {
        LeaderboardColumn() = default;

        hstring DisplayName();
        hstring StatisticName();
        Windows::Foundation::PropertyType StatisticType();
    };
}
