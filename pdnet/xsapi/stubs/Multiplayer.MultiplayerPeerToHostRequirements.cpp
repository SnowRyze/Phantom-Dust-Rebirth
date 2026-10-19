#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerPeerToHostRequirements.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Windows::Foundation::TimeSpan MultiplayerPeerToHostRequirements::LatencyMaximum()
    {
        PDNET_XSAPI_STUB("MultiplayerPeerToHostRequirements::LatencyMaximum");
    }
    uint64_t MultiplayerPeerToHostRequirements::BandwidthDownMinimumInKilobitsPerSecond()
    {
        PDNET_XSAPI_STUB("MultiplayerPeerToHostRequirements::BandwidthDownMinimumInKilobitsPerSecond");
    }
    uint64_t MultiplayerPeerToHostRequirements::BandwidthUpMinimumInKilobitsPerSecond()
    {
        PDNET_XSAPI_STUB("MultiplayerPeerToHostRequirements::BandwidthUpMinimumInKilobitsPerSecond");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayMetrics MultiplayerPeerToHostRequirements::HostSelectionMetric()
    {
        PDNET_XSAPI_STUB("MultiplayerPeerToHostRequirements::HostSelectionMetric");
    }
}
