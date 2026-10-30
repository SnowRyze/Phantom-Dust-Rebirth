#pragma once
#include "Tournaments/CurrentMatchMetadata.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct CurrentMatchMetadata : CurrentMatchMetadataT<CurrentMatchMetadata>
    {
        CurrentMatchMetadata() = default;

        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference GameSessionReference();
        Microsoft::Xbox::Services::Tournaments::MatchMetadata MatchDetails();
    };
}
