#pragma once
#include "Multiplayer/MultiplayerSessionChangeEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionChangeEventArgs : MultiplayerSessionChangeEventArgsT<MultiplayerSessionChangeEventArgs>
    {
        MultiplayerSessionChangeEventArgs() = default;

        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        hstring Branch();
        uint64_t ChangeNumber();
    };
}
