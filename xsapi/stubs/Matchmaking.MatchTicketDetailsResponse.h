#pragma once
#include "Matchmaking/MatchTicketDetailsResponse.g.h"


namespace winrt::Microsoft::Xbox::Services::Matchmaking::implementation
{
    struct MatchTicketDetailsResponse : MatchTicketDetailsResponseT<MatchTicketDetailsResponse>
    {
        MatchTicketDetailsResponse() = default;

        Microsoft::Xbox::Services::Matchmaking::TicketStatus MatchStatus();
        Windows::Foundation::TimeSpan EstimatedWaitTime();
        Microsoft::Xbox::Services::Matchmaking::PreserveSessionMode PreserveSession();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference TicketSession();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference TargetSession();
        hstring TicketAttributes();
    };
}
