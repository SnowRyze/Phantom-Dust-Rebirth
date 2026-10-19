#pragma once
#include "Matchmaking/CreateMatchTicketResponse.g.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    struct CreateMatchTicketResponse : CreateMatchTicketResponseT<CreateMatchTicketResponse>
    {
        CreateMatchTicketResponse() = default;

        hstring MatchTicketId();
        Windows::Foundation::TimeSpan EstimatedWaitTime();
    };
}
