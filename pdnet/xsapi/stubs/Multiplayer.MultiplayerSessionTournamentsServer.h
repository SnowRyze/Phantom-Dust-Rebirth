#pragma once
#include "Multiplayer/MultiplayerSessionTournamentsServer.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionTournamentsServer : MultiplayerSessionTournamentsServerT<MultiplayerSessionTournamentsServer>
    {
        MultiplayerSessionTournamentsServer() = default;

        Microsoft::Xbox::Services::Tournaments::TournamentReference TournamentReference();
        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference> Teams();
        Microsoft::Xbox::Services::Tournaments::TournamentRegistrationState RegistrationState();
        Microsoft::Xbox::Services::Tournaments::TournamentRegistrationReason RegistrationReason();
        Windows::Foundation::DateTime NextGameStartTime();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference NextGameSessionRef();
        Windows::Foundation::DateTime LastGameEndTime();
        Microsoft::Xbox::Services::Tournaments::TournamentTeamResult LastTeamResult();
        Microsoft::Xbox::Services::Tournaments::TournamentGameResultSource LastGameResultSource();
    };
}
