#pragma once
#include "Tournaments/TournamentService.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TournamentService : TournamentServiceT<TournamentService>
    {
        TournamentService() = default;

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TournamentRequestResult> GetTournamentsAsync(Microsoft::Xbox::Services::Tournaments::TournamentRequest request);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::Tournament> GetTournamentDetailsAsync(hstring organizerId, hstring tournamentId);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TeamRequestResult> GetTeamsAsync(Microsoft::Xbox::Services::Tournaments::TeamRequest request);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TeamInfo> GetTeamDetailsAsync(hstring organizerId, hstring tournamentId, hstring teamId);
        winrt::event_token TeamChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Tournaments::TeamChangeEventArgs> const& __param0);
        void TeamChanged(winrt::event_token const& __param0) noexcept;
        Microsoft::Xbox::Services::Tournaments::TeamChangeSubscription SubscribeToTeamChange(hstring const& organizerId, hstring const& tournamentId, hstring const& teamId);
        void UnsubscribeFromTeamChange(Microsoft::Xbox::Services::Tournaments::TeamChangeSubscription const& subscription);
    };
}
