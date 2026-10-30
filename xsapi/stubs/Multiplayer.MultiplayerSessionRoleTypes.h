#pragma once
#include "Multiplayer/MultiplayerSessionRoleTypes.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionRoleTypes : MultiplayerSessionRoleTypesT<MultiplayerSessionRoleTypes>
    {
        MultiplayerSessionRoleTypes() = default;

        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> RoleTypes();
    };
}
