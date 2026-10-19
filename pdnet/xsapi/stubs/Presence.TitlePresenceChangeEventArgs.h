#pragma once
#include "Presence/TitlePresenceChangeEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct TitlePresenceChangeEventArgs : TitlePresenceChangeEventArgsT<TitlePresenceChangeEventArgs>
    {
        TitlePresenceChangeEventArgs() = default;

        hstring XboxUserId();
        uint32_t TitleId();
        Microsoft::Xbox::Services::Presence::TitlePresenceState TitleState();
    };
}
