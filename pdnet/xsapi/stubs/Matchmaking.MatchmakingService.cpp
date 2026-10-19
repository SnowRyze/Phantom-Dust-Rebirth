#include "xsapi_pch.h"
#include "Matchmaking.MatchmakingService.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Matchmaking::CreateMatchTicketResponse> MatchmakingService::CreateMatchTicketAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference ticketSessionReference, hstring matchmakingServiceConfigurationId, hstring hopperName, Windows::Foundation::TimeSpan ticketTimeout, Microsoft::Xbox::Services::Matchmaking::PreserveSessionMode preserveSession, hstring ticketAttributesJson)
    {
        PDNET_XSAPI_STUB("MatchmakingService::CreateMatchTicketAsync");
    }
    Windows::Foundation::IAsyncAction MatchmakingService::DeleteMatchTicketAsync(hstring serviceConfigurationId, hstring hopperName, hstring ticketId)
    {
        PDNET_XSAPI_STUB("MatchmakingService::DeleteMatchTicketAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Matchmaking::MatchTicketDetailsResponse> MatchmakingService::GetMatchTicketDetailsAsync(hstring serviceConfigurationId, hstring hopperName, hstring ticketId)
    {
        PDNET_XSAPI_STUB("MatchmakingService::GetMatchTicketDetailsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Matchmaking::HopperStatisticsResponse> MatchmakingService::GetHopperStatisticsAsync(hstring serviceConfigurationId, hstring hopperName)
    {
        PDNET_XSAPI_STUB("MatchmakingService::GetHopperStatisticsAsync");
    }
}
