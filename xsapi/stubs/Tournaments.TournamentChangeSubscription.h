#pragma once
#include "Tournaments/TournamentChangeSubscription.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TournamentChangeSubscription : TournamentChangeSubscriptionT<TournamentChangeSubscription>
    {
        TournamentChangeSubscription() = default;

        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState State();
        hstring OrganizerId();
        hstring TournamentId();
    };
}
