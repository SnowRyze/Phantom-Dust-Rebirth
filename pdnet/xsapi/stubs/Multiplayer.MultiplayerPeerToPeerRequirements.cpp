#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerPeerToPeerRequirements.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Windows::Foundation::TimeSpan MultiplayerPeerToPeerRequirements::LatencyMaximum()
    {
        PDNET_XSAPI_STUB("MultiplayerPeerToPeerRequirements::LatencyMaximum");
    }
    uint64_t MultiplayerPeerToPeerRequirements::BandwidthMinimumInKilobitsPerSecond()
    {
        PDNET_XSAPI_STUB("MultiplayerPeerToPeerRequirements::BandwidthMinimumInKilobitsPerSecond");
    }
}
