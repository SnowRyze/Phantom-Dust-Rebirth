#pragma once
#include "Tournaments/TournamentChangeEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TournamentChangeEventArgs : TournamentChangeEventArgsT<TournamentChangeEventArgs>
    {
        TournamentChangeEventArgs() = default;

        hstring OrganizerId();
        hstring TournamentId();
    };
}
