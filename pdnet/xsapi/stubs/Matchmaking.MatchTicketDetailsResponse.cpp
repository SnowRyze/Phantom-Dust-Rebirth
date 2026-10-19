#include "xsapi_pch.h"
#include "Matchmaking.MatchTicketDetailsResponse.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    Microsoft::Xbox::Services::Matchmaking::TicketStatus MatchTicketDetailsResponse::MatchStatus()
    {
        PDNET_XSAPI_STUB("MatchTicketDetailsResponse::MatchStatus");
    }
    Windows::Foundation::TimeSpan MatchTicketDetailsResponse::EstimatedWaitTime()
    {
        PDNET_XSAPI_STUB("MatchTicketDetailsResponse::EstimatedWaitTime");
    }
    Microsoft::Xbox::Services::Matchmaking::PreserveSessionMode MatchTicketDetailsResponse::PreserveSession()
    {
        PDNET_XSAPI_STUB("MatchTicketDetailsResponse::PreserveSession");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MatchTicketDetailsResponse::TicketSession()
    {
        PDNET_XSAPI_STUB("MatchTicketDetailsResponse::TicketSession");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MatchTicketDetailsResponse::TargetSession()
    {
        PDNET_XSAPI_STUB("MatchTicketDetailsResponse::TargetSession");
    }
    hstring MatchTicketDetailsResponse::TicketAttributes()
    {
        PDNET_XSAPI_STUB("MatchTicketDetailsResponse::TicketAttributes");
    }
}
