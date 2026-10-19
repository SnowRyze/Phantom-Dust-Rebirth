#include "xsapi_pch.h"
#include "Leaderboard.LeaderboardColumn.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    hstring LeaderboardColumn::DisplayName()
    {
        PDNET_XSAPI_STUB("LeaderboardColumn::DisplayName");
    }
    hstring LeaderboardColumn::StatisticName()
    {
        PDNET_XSAPI_STUB("LeaderboardColumn::StatisticName");
    }
    Windows::Foundation::PropertyType LeaderboardColumn::StatisticType()
    {
        PDNET_XSAPI_STUB("LeaderboardColumn::StatisticType");
    }
}
