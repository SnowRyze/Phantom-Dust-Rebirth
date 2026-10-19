#pragma once
#include "Multiplayer/WriteSessionResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct WriteSessionResult : WriteSessionResultT<WriteSessionResult>
    {
        WriteSessionResult() = default;

        bool Succeeded();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession Session();
        Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus Status();
    };
}
