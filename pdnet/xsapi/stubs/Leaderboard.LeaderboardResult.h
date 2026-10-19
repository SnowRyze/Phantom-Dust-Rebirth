#pragma once
#include "Leaderboard/LeaderboardResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardResult : LeaderboardResultT<LeaderboardResult>
    {
        LeaderboardResult() = default;

        hstring DisplayName();
        uint32_t TotalRowCount();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn> Columns();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow> Rows();
        bool HasNext();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> GetNextAsync(uint32_t maxItems);
        Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery GetNextQuery();
    };
}
