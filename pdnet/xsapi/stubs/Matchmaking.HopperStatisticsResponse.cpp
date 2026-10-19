#include "xsapi_pch.h"
#include "Matchmaking.HopperStatisticsResponse.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    hstring HopperStatisticsResponse::HopperName()
    {
        PDNET_XSAPI_STUB("HopperStatisticsResponse::HopperName");
    }
    Windows::Foundation::TimeSpan HopperStatisticsResponse::EstimatedWaitTime()
    {
        PDNET_XSAPI_STUB("HopperStatisticsResponse::EstimatedWaitTime");
    }
    uint32_t HopperStatisticsResponse::PlayersWaitingToMatch()
    {
        PDNET_XSAPI_STUB("HopperStatisticsResponse::PlayersWaitingToMatch");
    }
}
