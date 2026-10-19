#pragma once
#include "GameServerPlatform/GameServerPortMapping.g.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    struct GameServerPortMapping : GameServerPortMappingT<GameServerPortMapping>
    {
        GameServerPortMapping() = default;

        hstring PortName();
        uint32_t InternalPortNumber();
        uint32_t ExternalPortNumber();
    };
}
