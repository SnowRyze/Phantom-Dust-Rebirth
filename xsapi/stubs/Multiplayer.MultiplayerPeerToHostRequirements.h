#pragma once
#include "Multiplayer/MultiplayerPeerToHostRequirements.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerPeerToHostRequirements : MultiplayerPeerToHostRequirementsT<MultiplayerPeerToHostRequirements>
    {
        MultiplayerPeerToHostRequirements() = default;

        Windows::Foundation::TimeSpan LatencyMaximum();
        uint64_t BandwidthDownMinimumInKilobitsPerSecond();
        uint64_t BandwidthUpMinimumInKilobitsPerSecond();
        Microsoft::Xbox::Services::Multiplayer::MultiplayMetrics HostSelectionMetric();
    };
}
