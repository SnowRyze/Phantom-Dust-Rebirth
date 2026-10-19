#pragma once
#include "Multiplayer/MultiplayerSessionArbitrationServer.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionArbitrationServer : MultiplayerSessionArbitrationServerT<MultiplayerSessionArbitrationServer>
    {
        MultiplayerSessionArbitrationServer() = default;

        Windows::Foundation::DateTime ArbitrationStartTime();
        Microsoft::Xbox::Services::Tournaments::TournamentArbitrationState ResultState();
        Microsoft::Xbox::Services::Tournaments::TournamentGameResultSource ResultSource();
        uint32_t ResultConfidenceLevel();
        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> Results();
    };
}
