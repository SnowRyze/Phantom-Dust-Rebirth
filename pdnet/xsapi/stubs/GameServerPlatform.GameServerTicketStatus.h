#pragma once
#include "GameServerPlatform/GameServerTicketStatus.g.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    struct GameServerTicketStatus : GameServerTicketStatusT<GameServerTicketStatus>
    {
        GameServerTicketStatus() = default;

        hstring TicketId();
        hstring ClusterId();
        uint32_t TitleId();
        hstring HostName();
        Microsoft::Xbox::Services::GameServerPlatform::GameServerHostStatus Status();
        hstring Description();
        hstring SecureContext();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerPortMapping> PortMappings();
        hstring GameHostId();
        hstring Region();
    };
}
