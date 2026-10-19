#include "xsapi_pch.h"
#include "Tournaments.CurrentMatchMetadata.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference CurrentMatchMetadata::GameSessionReference()
    {
        PDNET_XSAPI_STUB("CurrentMatchMetadata::GameSessionReference");
    }
    Microsoft::Xbox::Services::Tournaments::MatchMetadata CurrentMatchMetadata::MatchDetails()
    {
        PDNET_XSAPI_STUB("CurrentMatchMetadata::MatchDetails");
    }
}
