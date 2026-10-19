#pragma once
#include "Leaderboard/LeaderboardRow.g.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardRow : LeaderboardRowT<LeaderboardRow>
    {
        LeaderboardRow() = default;

        hstring Gamertag();
        hstring XboxUserId();
        double Percentile();
        uint32_t Rank();
        Windows::Foundation::Collections::IVectorView<hstring> Values();
    };
}
