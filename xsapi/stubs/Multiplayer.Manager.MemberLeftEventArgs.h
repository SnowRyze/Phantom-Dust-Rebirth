#pragma once
#include "Multiplayer/Manager/MemberLeftEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct MemberLeftEventArgs : MemberLeftEventArgsT<MemberLeftEventArgs>
    {
        MemberLeftEventArgs() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> Members();
    };
}
