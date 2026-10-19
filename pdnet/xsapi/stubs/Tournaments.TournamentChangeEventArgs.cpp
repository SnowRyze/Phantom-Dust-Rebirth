#include "xsapi_pch.h"
#include "Tournaments.TournamentChangeEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    hstring TournamentChangeEventArgs::OrganizerId()
    {
        PDNET_XSAPI_STUB("TournamentChangeEventArgs::OrganizerId");
    }
    hstring TournamentChangeEventArgs::TournamentId()
    {
        PDNET_XSAPI_STUB("TournamentChangeEventArgs::TournamentId");
    }
}
