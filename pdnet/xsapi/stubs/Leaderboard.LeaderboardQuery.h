#pragma once
#include "Leaderboard/LeaderboardQuery.g.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardQuery : LeaderboardQueryT<LeaderboardQuery>
    {
        LeaderboardQuery() = default;

        bool SkipResultToMe();
        void SkipResultToMe(bool value);
        uint32_t SkipResultToRank();
        void SkipResultToRank(uint32_t value);
        uint32_t MaxItems();
        void MaxItems(uint32_t value);
        Microsoft::Xbox::Services::Leaderboard::SortOrder Order();
        void Order(Microsoft::Xbox::Services::Leaderboard::SortOrder const& value);
        hstring StatName();
        hstring SocialGroup();
        bool HasNext();
    };
}
