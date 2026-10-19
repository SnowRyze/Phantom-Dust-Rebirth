#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerRoleInfo.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerRoleInfo::MemberXboxUserIds()
    {
        PDNET_XSAPI_STUB("MultiplayerRoleInfo::MemberXboxUserIds");
    }
    uint32_t MultiplayerRoleInfo::MembersCount()
    {
        PDNET_XSAPI_STUB("MultiplayerRoleInfo::MembersCount");
    }
    uint32_t MultiplayerRoleInfo::TargetCount()
    {
        PDNET_XSAPI_STUB("MultiplayerRoleInfo::TargetCount");
    }
    void MultiplayerRoleInfo::TargetCount(uint32_t value)
    {
        PDNET_XSAPI_STUB("MultiplayerRoleInfo::TargetCount");
    }
    uint32_t MultiplayerRoleInfo::MaxMembersCount()
    {
        PDNET_XSAPI_STUB("MultiplayerRoleInfo::MaxMembersCount");
    }
    void MultiplayerRoleInfo::MaxMembersCount(uint32_t value)
    {
        PDNET_XSAPI_STUB("MultiplayerRoleInfo::MaxMembersCount");
    }
}
