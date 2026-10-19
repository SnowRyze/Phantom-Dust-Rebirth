#include "xsapi_pch.h"
#include "Leaderboard.LeaderboardResult.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    hstring LeaderboardResult::DisplayName()
    {
        PDNET_XSAPI_STUB("LeaderboardResult::DisplayName");
    }
    uint32_t LeaderboardResult::TotalRowCount()
    {
        PDNET_XSAPI_STUB("LeaderboardResult::TotalRowCount");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn> LeaderboardResult::Columns()
    {
        PDNET_XSAPI_STUB("LeaderboardResult::Columns");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow> LeaderboardResult::Rows()
    {
        PDNET_XSAPI_STUB("LeaderboardResult::Rows");
    }
    bool LeaderboardResult::HasNext()
    {
        PDNET_XSAPI_STUB("LeaderboardResult::HasNext");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardResult::GetNextAsync(uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardResult::GetNextAsync");
    }
    Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery LeaderboardResult::GetNextQuery()
    {
        PDNET_XSAPI_STUB("LeaderboardResult::GetNextQuery");
    }
}
