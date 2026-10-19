#include "xsapi_pch.h"
#include "GameServerPlatform.GameServerPortMapping.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    hstring GameServerPortMapping::PortName()
    {
        PDNET_XSAPI_STUB("GameServerPortMapping::PortName");
    }
    uint32_t GameServerPortMapping::InternalPortNumber()
    {
        PDNET_XSAPI_STUB("GameServerPortMapping::InternalPortNumber");
    }
    uint32_t GameServerPortMapping::ExternalPortNumber()
    {
        PDNET_XSAPI_STUB("GameServerPortMapping::ExternalPortNumber");
    }
}
