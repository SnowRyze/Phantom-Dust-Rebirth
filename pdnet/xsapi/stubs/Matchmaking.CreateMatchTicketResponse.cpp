#include "xsapi_pch.h"
#include "Matchmaking.CreateMatchTicketResponse.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    hstring CreateMatchTicketResponse::MatchTicketId()
    {
        PDNET_XSAPI_STUB("CreateMatchTicketResponse::MatchTicketId");
    }
    Windows::Foundation::TimeSpan CreateMatchTicketResponse::EstimatedWaitTime()
    {
        PDNET_XSAPI_STUB("CreateMatchTicketResponse::EstimatedWaitTime");
    }
}
