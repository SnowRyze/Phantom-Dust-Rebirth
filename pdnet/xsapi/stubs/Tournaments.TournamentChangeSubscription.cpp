#include "xsapi_pch.h"
#include "Tournaments.TournamentChangeSubscription.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState TournamentChangeSubscription::State()
    {
        PDNET_XSAPI_STUB("TournamentChangeSubscription::State");
    }
    hstring TournamentChangeSubscription::OrganizerId()
    {
        PDNET_XSAPI_STUB("TournamentChangeSubscription::OrganizerId");
    }
    hstring TournamentChangeSubscription::TournamentId()
    {
        PDNET_XSAPI_STUB("TournamentChangeSubscription::TournamentId");
    }
}
