#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionProperties.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSessionProperties::Keywords()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::Keywords");
    }
    void MultiplayerSessionProperties::Keywords(Windows::Foundation::Collections::IVectorView<hstring> const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::Keywords");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction MultiplayerSessionProperties::JoinRestriction()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::JoinRestriction");
    }
    void MultiplayerSessionProperties::JoinRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::JoinRestriction");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction MultiplayerSessionProperties::ReadRestriction()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::ReadRestriction");
    }
    void MultiplayerSessionProperties::ReadRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::ReadRestriction");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> MultiplayerSessionProperties::TurnCollection()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::TurnCollection");
    }
    void MultiplayerSessionProperties::TurnCollection(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::TurnCollection");
    }
    hstring MultiplayerSessionProperties::MatchmakingTargetSessionConstantsJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::MatchmakingTargetSessionConstantsJson");
    }
    hstring MultiplayerSessionProperties::SessionCustomPropertiesJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::SessionCustomPropertiesJson");
    }
    hstring MultiplayerSessionProperties::MatchmakingServerConnectionString()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::MatchmakingServerConnectionString");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSessionProperties::ServerConnectionStringCandidates()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::ServerConnectionStringCandidates");
    }
    Windows::Foundation::Collections::IVectorView<uint32_t> MultiplayerSessionProperties::SessionOwnerIndices()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::SessionOwnerIndices");
    }
    hstring MultiplayerSessionProperties::HostDeviceToken()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::HostDeviceToken");
    }
    bool MultiplayerSessionProperties::Closed()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::Closed");
    }
    bool MultiplayerSessionProperties::AllocateCloudCompute()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionProperties::AllocateCloudCompute");
    }
}
