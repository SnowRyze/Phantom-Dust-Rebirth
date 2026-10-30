#pragma once
#include "Multiplayer/MultiplayerSubscriptionLostEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSubscriptionLostEventArgs : MultiplayerSubscriptionLostEventArgsT<MultiplayerSubscriptionLostEventArgs>
    {
        MultiplayerSubscriptionLostEventArgs() = default;

    };
}
