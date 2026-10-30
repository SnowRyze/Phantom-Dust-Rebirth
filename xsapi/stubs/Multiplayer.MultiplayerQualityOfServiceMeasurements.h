#pragma once
#include "Multiplayer/MultiplayerQualityOfServiceMeasurements.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerQualityOfServiceMeasurements : MultiplayerQualityOfServiceMeasurementsT<MultiplayerQualityOfServiceMeasurements>
    {
        MultiplayerQualityOfServiceMeasurements() = default;

        MultiplayerQualityOfServiceMeasurements(hstring const& memberDeviceToken, Windows::Foundation::TimeSpan const& latency, uint64_t bandwidthDownInKilobitsPerSecond, uint64_t bandwidthUpInKilobitsPerSecond, hstring const& customJson);
        hstring MemberDeviceToken();
        Windows::Foundation::TimeSpan Latency();
        uint64_t BandwidthDownInKilobitsPerSecond();
        uint64_t BandwidthUpInKilobitsPerSecond();
        hstring CustomJson();
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerQualityOfServiceMeasurements : MultiplayerQualityOfServiceMeasurementsT<MultiplayerQualityOfServiceMeasurements, implementation::MultiplayerQualityOfServiceMeasurements>
    {
    };
}
