#include "xsapi_pch.h"
#include "Leaderboard.LeaderboardRow.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    hstring LeaderboardRow::Gamertag()
    {
        PDNET_XSAPI_STUB("LeaderboardRow::Gamertag");
    }
    hstring LeaderboardRow::XboxUserId()
    {
        PDNET_XSAPI_STUB("LeaderboardRow::XboxUserId");
    }
    double LeaderboardRow::Percentile()
    {
        PDNET_XSAPI_STUB("LeaderboardRow::Percentile");
    }
    uint32_t LeaderboardRow::Rank()
    {
        PDNET_XSAPI_STUB("LeaderboardRow::Rank");
    }
    Windows::Foundation::Collections::IVectorView<hstring> LeaderboardRow::Values()
    {
        PDNET_XSAPI_STUB("LeaderboardRow::Values");
    }
}
