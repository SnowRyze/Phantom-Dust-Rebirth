#include "xsapi_pch.h"
#include "Tournaments.TeamChangeSubscription.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState TeamChangeSubscription::State()
    {
        PDNET_XSAPI_STUB("TeamChangeSubscription::State");
    }
    hstring TeamChangeSubscription::OrganizerId()
    {
        PDNET_XSAPI_STUB("TeamChangeSubscription::OrganizerId");
    }
    hstring TeamChangeSubscription::TournamentId()
    {
        PDNET_XSAPI_STUB("TeamChangeSubscription::TournamentId");
    }
    hstring TeamChangeSubscription::TeamId()
    {
        PDNET_XSAPI_STUB("TeamChangeSubscription::TeamId");
    }
}
