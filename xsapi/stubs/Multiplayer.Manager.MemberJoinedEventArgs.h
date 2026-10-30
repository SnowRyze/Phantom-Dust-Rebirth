#pragma once
#include "Multiplayer/Manager/MemberJoinedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct MemberJoinedEventArgs : MemberJoinedEventArgsT<MemberJoinedEventArgs>
    {
        MemberJoinedEventArgs() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> Members();
    };
}
