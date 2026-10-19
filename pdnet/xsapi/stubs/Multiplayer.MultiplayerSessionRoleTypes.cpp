#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionRoleTypes.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> MultiplayerSessionRoleTypes::RoleTypes()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionRoleTypes::RoleTypes");
    }
}
