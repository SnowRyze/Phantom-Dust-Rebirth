#pragma once
#include "Multiplayer/MultiplayerPeerToPeerRequirements.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerPeerToPeerRequirements : MultiplayerPeerToPeerRequirementsT<MultiplayerPeerToPeerRequirements>
    {
        MultiplayerPeerToPeerRequirements() = default;

        Windows::Foundation::TimeSpan LatencyMaximum();
        uint64_t BandwidthMinimumInKilobitsPerSecond();
    };
}
