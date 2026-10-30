#pragma once
#include "Tournaments/PreviousMatchMetadata.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct PreviousMatchMetadata : PreviousMatchMetadataT<PreviousMatchMetadata>
    {
        PreviousMatchMetadata() = default;

        Microsoft::Xbox::Services::Tournaments::TournamentTeamResult Result();
        Windows::Foundation::DateTime EndTime();
        Microsoft::Xbox::Services::Tournaments::MatchMetadata MatchDetails();
    };
}
