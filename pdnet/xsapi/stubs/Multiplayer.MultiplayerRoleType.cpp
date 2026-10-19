#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerRoleType.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    bool MultiplayerRoleType::OwnerManaged()
    {
        PDNET_XSAPI_STUB("MultiplayerRoleType::OwnerManaged");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MutableRoleSetting> MultiplayerRoleType::MutableRoleSettings()
    {
        PDNET_XSAPI_STUB("MultiplayerRoleType::MutableRoleSettings");
    }
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleInfo> MultiplayerRoleType::Roles()
    {
        PDNET_XSAPI_STUB("MultiplayerRoleType::Roles");
    }
    void MultiplayerRoleType::Roles(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleInfo> const& roles)
    {
        PDNET_XSAPI_STUB("MultiplayerRoleType::Roles");
    }
}
