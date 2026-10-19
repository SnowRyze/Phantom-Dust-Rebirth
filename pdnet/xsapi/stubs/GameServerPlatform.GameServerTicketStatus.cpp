#include "xsapi_pch.h"
#include "GameServerPlatform.GameServerTicketStatus.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    hstring GameServerTicketStatus::TicketId()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::TicketId");
    }
    hstring GameServerTicketStatus::ClusterId()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::ClusterId");
    }
    uint32_t GameServerTicketStatus::TitleId()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::TitleId");
    }
    hstring GameServerTicketStatus::HostName()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::HostName");
    }
    Microsoft::Xbox::Services::GameServerPlatform::GameServerHostStatus GameServerTicketStatus::Status()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::Status");
    }
    hstring GameServerTicketStatus::Description()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::Description");
    }
    hstring GameServerTicketStatus::SecureContext()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::SecureContext");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerPortMapping> GameServerTicketStatus::PortMappings()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::PortMappings");
    }
    hstring GameServerTicketStatus::GameHostId()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::GameHostId");
    }
    hstring GameServerTicketStatus::Region()
    {
        PDNET_XSAPI_STUB("GameServerTicketStatus::Region");
    }
}
