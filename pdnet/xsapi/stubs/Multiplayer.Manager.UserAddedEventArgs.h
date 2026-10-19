#pragma once
#include "Multiplayer/Manager/UserAddedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct UserAddedEventArgs : UserAddedEventArgsT<UserAddedEventArgs>
    {
        UserAddedEventArgs() = default;

        hstring XboxUserId();
    };
}
