#pragma once
#include "Multiplayer/Manager/TournamentRegistrationStateChangedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct TournamentRegistrationStateChangedEventArgs : TournamentRegistrationStateChangedEventArgsT<TournamentRegistrationStateChangedEventArgs>
    {
        TournamentRegistrationStateChangedEventArgs() = default;

        Microsoft::Xbox::Services::Tournaments::TournamentRegistrationState RegistrationState();
        Microsoft::Xbox::Services::Tournaments::TournamentRegistrationReason RegistrationReason();
    };
}
