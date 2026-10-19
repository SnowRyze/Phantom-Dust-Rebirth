#include "xsapi_pch.h"
#include "GameServerPlatform.AllocationResult.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    Microsoft::Xbox::Services::GameServerPlatform::GameServerFulfillmentState AllocationResult::FulfillmentState()
    {
        PDNET_XSAPI_STUB("AllocationResult::FulfillmentState");
    }
    hstring AllocationResult::HostName()
    {
        PDNET_XSAPI_STUB("AllocationResult::HostName");
    }
    hstring AllocationResult::SessionHostId()
    {
        PDNET_XSAPI_STUB("AllocationResult::SessionHostId");
    }
    hstring AllocationResult::Region()
    {
        PDNET_XSAPI_STUB("AllocationResult::Region");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerPortMapping> AllocationResult::PortMappings()
    {
        PDNET_XSAPI_STUB("AllocationResult::PortMappings");
    }
    hstring AllocationResult::SecureDeviceAddress()
    {
        PDNET_XSAPI_STUB("AllocationResult::SecureDeviceAddress");
    }
}
