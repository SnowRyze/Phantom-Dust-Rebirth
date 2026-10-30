#pragma once
#include "Matchmaking/MatchmakingService.g.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    struct MatchmakingService : MatchmakingServiceT<MatchmakingService>
    {
        MatchmakingService() = default;

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Matchmaking::CreateMatchTicketResponse> CreateMatchTicketAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference ticketSessionReference, hstring matchmakingServiceConfigurationId, hstring hopperName, Windows::Foundation::TimeSpan ticketTimeout, Microsoft::Xbox::Services::Matchmaking::PreserveSessionMode preserveSession, hstring ticketAttributesJson);
        Windows::Foundation::IAsyncAction DeleteMatchTicketAsync(hstring serviceConfigurationId, hstring hopperName, hstring ticketId);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Matchmaking::MatchTicketDetailsResponse> GetMatchTicketDetailsAsync(hstring serviceConfigurationId, hstring hopperName, hstring ticketId);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Matchmaking::HopperStatisticsResponse> GetHopperStatisticsAsync(hstring serviceConfigurationId, hstring hopperName);
    };
}
