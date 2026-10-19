#pragma once
#include "GameServerPlatform/AllocationResult.g.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    struct AllocationResult : AllocationResultT<AllocationResult>
    {
        AllocationResult() = default;

        Microsoft::Xbox::Services::GameServerPlatform::GameServerFulfillmentState FulfillmentState();
        hstring HostName();
        hstring SessionHostId();
        hstring Region();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerPortMapping> PortMappings();
        hstring SecureDeviceAddress();
    };
}
