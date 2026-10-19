#pragma once
#include "Multiplayer/Manager/HostChangedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct HostChangedEventArgs : HostChangedEventArgsT<HostChangedEventArgs>
    {
        HostChangedEventArgs() = default;

        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember HostMember();
    };
}
