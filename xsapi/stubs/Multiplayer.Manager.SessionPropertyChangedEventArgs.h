#pragma once
#include "Multiplayer/Manager/SessionPropertyChangedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct SessionPropertyChangedEventArgs : SessionPropertyChangedEventArgsT<SessionPropertyChangedEventArgs>
    {
        SessionPropertyChangedEventArgs() = default;

        hstring Properties();
    };
}
