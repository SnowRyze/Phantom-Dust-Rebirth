#include "xsapi_pch.h"
#include "Multiplayer.Manager.MultiplayerMember.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    uint32_t MultiplayerMember::MemberId()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::MemberId");
    }
    hstring MultiplayerMember::TeamId()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::TeamId");
    }
    hstring MultiplayerMember::XboxUserId()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::XboxUserId");
    }
    hstring MultiplayerMember::DebugGamertag()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::DebugGamertag");
    }
    bool MultiplayerMember::IsLocal()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::IsLocal");
    }
    bool MultiplayerMember::IsInLobby()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::IsInLobby");
    }
    bool MultiplayerMember::IsInGame()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::IsInGame");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus MultiplayerMember::Status()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::Status");
    }
    hstring MultiplayerMember::ConnectionAddress()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::ConnectionAddress");
    }
    hstring MultiplayerMember::Properties()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::Properties");
    }
    bool MultiplayerMember::IsMemberOnSameDevice(Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember const& member)
    {
        PDNET_XSAPI_STUB("MultiplayerMember::IsMemberOnSameDevice");
    }
    hstring MultiplayerMember::_DeviceToken()
    {
        PDNET_XSAPI_STUB("MultiplayerMember::_DeviceToken");
    }
}
