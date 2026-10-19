#pragma once
#include "Multiplayer/MultiplayerSessionMatchmakingServer.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionMatchmakingServer : MultiplayerSessionMatchmakingServerT<MultiplayerSessionMatchmakingServer>
    {
        MultiplayerSessionMatchmakingServer() = default;

        Microsoft::Xbox::Services::Multiplayer::MatchmakingStatus Status();
        hstring StatusDetails();
        Windows::Foundation::TimeSpan TypicalWait();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference TargetSessionRef();
    };
}
