#include "xsapi_pch.h"
#include "GameServerPlatform.ClusterResult.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    Windows::Foundation::TimeSpan ClusterResult::PollInterval()
    {
        PDNET_XSAPI_STUB("ClusterResult::PollInterval");
    }
    Microsoft::Xbox::Services::GameServerPlatform::GameServerFulfillmentState ClusterResult::FulfillmentState()
    {
        PDNET_XSAPI_STUB("ClusterResult::FulfillmentState");
    }
    hstring ClusterResult::HostName()
    {
        PDNET_XSAPI_STUB("ClusterResult::HostName");
    }
    hstring ClusterResult::Region()
    {
        PDNET_XSAPI_STUB("ClusterResult::Region");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerPortMapping> ClusterResult::PortMappings()
    {
        PDNET_XSAPI_STUB("ClusterResult::PortMappings");
    }
    hstring ClusterResult::SecureDeviceAddress()
    {
        PDNET_XSAPI_STUB("ClusterResult::SecureDeviceAddress");
    }
}
