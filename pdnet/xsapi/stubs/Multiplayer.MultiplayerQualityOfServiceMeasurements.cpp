#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerQualityOfServiceMeasurements.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    MultiplayerQualityOfServiceMeasurements::MultiplayerQualityOfServiceMeasurements(hstring const& memberDeviceToken, Windows::Foundation::TimeSpan const& latency, uint64_t bandwidthDownInKilobitsPerSecond, uint64_t bandwidthUpInKilobitsPerSecond, hstring const& customJson)
    {
        PDNET_XSAPI_STUB("MultiplayerQualityOfServiceMeasurements::MultiplayerQualityOfServiceMeasurements");
    }
    hstring MultiplayerQualityOfServiceMeasurements::MemberDeviceToken()
    {
        PDNET_XSAPI_STUB("MultiplayerQualityOfServiceMeasurements::MemberDeviceToken");
    }
    Windows::Foundation::TimeSpan MultiplayerQualityOfServiceMeasurements::Latency()
    {
        PDNET_XSAPI_STUB("MultiplayerQualityOfServiceMeasurements::Latency");
    }
    uint64_t MultiplayerQualityOfServiceMeasurements::BandwidthDownInKilobitsPerSecond()
    {
        PDNET_XSAPI_STUB("MultiplayerQualityOfServiceMeasurements::BandwidthDownInKilobitsPerSecond");
    }
    uint64_t MultiplayerQualityOfServiceMeasurements::BandwidthUpInKilobitsPerSecond()
    {
        PDNET_XSAPI_STUB("MultiplayerQualityOfServiceMeasurements::BandwidthUpInKilobitsPerSecond");
    }
    hstring MultiplayerQualityOfServiceMeasurements::CustomJson()
    {
        PDNET_XSAPI_STUB("MultiplayerQualityOfServiceMeasurements::CustomJson");
    }
}
