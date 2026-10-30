#pragma once
#include "Multiplayer/Manager/TournamentGameSessionReadyEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct TournamentGameSessionReadyEventArgs : TournamentGameSessionReadyEventArgsT<TournamentGameSessionReadyEventArgs>
    {
        TournamentGameSessionReadyEventArgs() = default;

        Windows::Foundation::DateTime StartTime();
    };
}
