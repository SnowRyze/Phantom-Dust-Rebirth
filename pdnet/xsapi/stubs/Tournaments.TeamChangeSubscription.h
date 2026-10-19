#pragma once
#include "Tournaments/TeamChangeSubscription.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TeamChangeSubscription : TeamChangeSubscriptionT<TeamChangeSubscription>
    {
        TeamChangeSubscription() = default;

        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState State();
        hstring OrganizerId();
        hstring TournamentId();
        hstring TeamId();
    };
}
