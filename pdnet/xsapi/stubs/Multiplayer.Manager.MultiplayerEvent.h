#pragma once
#include "Multiplayer/Manager/MultiplayerEvent.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct MultiplayerEvent : MultiplayerEventT<MultiplayerEvent>
    {
        MultiplayerEvent() = default;

        int32_t ErrorCode();
        hstring ErrorMessage();
        Windows::Foundation::IInspectable Context();
        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerEventType EventType();
        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerEventArgs EventArgs();
        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerSessionType SessionType();
    };
}
