#include "xsapi_pch.h"
#include "Tournaments.TeamChangeEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    hstring TeamChangeEventArgs::OrganizerId()
    {
        PDNET_XSAPI_STUB("TeamChangeEventArgs::OrganizerId");
    }
    hstring TeamChangeEventArgs::TournamentId()
    {
        PDNET_XSAPI_STUB("TeamChangeEventArgs::TournamentId");
    }
    hstring TeamChangeEventArgs::TeamId()
    {
        PDNET_XSAPI_STUB("TeamChangeEventArgs::TeamId");
    }
}
