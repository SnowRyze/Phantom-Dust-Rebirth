#pragma once
#include "Multiplayer/MultiplayerRoleInfo.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerRoleInfo : MultiplayerRoleInfoT<MultiplayerRoleInfo>
    {
        MultiplayerRoleInfo() = default;

        Windows::Foundation::Collections::IVectorView<hstring> MemberXboxUserIds();
        uint32_t MembersCount();
        uint32_t TargetCount();
        void TargetCount(uint32_t value);
        uint32_t MaxMembersCount();
        void MaxMembersCount(uint32_t value);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerRoleInfo : MultiplayerRoleInfoT<MultiplayerRoleInfo, implementation::MultiplayerRoleInfo>
    {
    };
}
