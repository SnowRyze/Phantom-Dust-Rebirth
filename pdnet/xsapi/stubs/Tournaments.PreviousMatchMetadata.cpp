#include "xsapi_pch.h"
#include "Tournaments.PreviousMatchMetadata.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Microsoft::Xbox::Services::Tournaments::TournamentTeamResult PreviousMatchMetadata::Result()
    {
        PDNET_XSAPI_STUB("PreviousMatchMetadata::Result");
    }
    Windows::Foundation::DateTime PreviousMatchMetadata::EndTime()
    {
        PDNET_XSAPI_STUB("PreviousMatchMetadata::EndTime");
    }
    Microsoft::Xbox::Services::Tournaments::MatchMetadata PreviousMatchMetadata::MatchDetails()
    {
        PDNET_XSAPI_STUB("PreviousMatchMetadata::MatchDetails");
    }
}
