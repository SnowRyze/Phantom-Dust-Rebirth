#pragma once
#include "Multiplayer/MultiplayerRoleType.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerRoleType : MultiplayerRoleTypeT<MultiplayerRoleType>
    {
        MultiplayerRoleType() = default;

        bool OwnerManaged();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MutableRoleSetting> MutableRoleSettings();
        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleInfo> Roles();
        void Roles(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleInfo> const& roles);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerRoleType : MultiplayerRoleTypeT<MultiplayerRoleType, implementation::MultiplayerRoleType>
    {
    };
}
