#include "xsapi_pch.h"
#include "Tournaments.TournamentService.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TournamentRequestResult> TournamentService::GetTournamentsAsync(Microsoft::Xbox::Services::Tournaments::TournamentRequest request)
    {
        PDNET_XSAPI_STUB("TournamentService::GetTournamentsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::Tournament> TournamentService::GetTournamentDetailsAsync(hstring organizerId, hstring tournamentId)
    {
        PDNET_XSAPI_STUB("TournamentService::GetTournamentDetailsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TeamRequestResult> TournamentService::GetTeamsAsync(Microsoft::Xbox::Services::Tournaments::TeamRequest request)
    {
        PDNET_XSAPI_STUB("TournamentService::GetTeamsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TeamInfo> TournamentService::GetTeamDetailsAsync(hstring organizerId, hstring tournamentId, hstring teamId)
    {
        PDNET_XSAPI_STUB("TournamentService::GetTeamDetailsAsync");
    }
    winrt::event_token TournamentService::TeamChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Tournaments::TeamChangeEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("TournamentService::TeamChanged");
    }
    void TournamentService::TeamChanged(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("TournamentService::TeamChanged");
    }
    Microsoft::Xbox::Services::Tournaments::TeamChangeSubscription TournamentService::SubscribeToTeamChange(hstring const& organizerId, hstring const& tournamentId, hstring const& teamId)
    {
        PDNET_XSAPI_STUB("TournamentService::SubscribeToTeamChange");
    }
    void TournamentService::UnsubscribeFromTeamChange(Microsoft::Xbox::Services::Tournaments::TeamChangeSubscription const& subscription)
    {
        PDNET_XSAPI_STUB("TournamentService::UnsubscribeFromTeamChange");
    }
}
