#pragma once
#include "Matchmaking/HopperStatisticsResponse.g.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    struct HopperStatisticsResponse : HopperStatisticsResponseT<HopperStatisticsResponse>
    {
        HopperStatisticsResponse() = default;

        hstring HopperName();
        Windows::Foundation::TimeSpan EstimatedWaitTime();
        uint32_t PlayersWaitingToMatch();
    };
}
