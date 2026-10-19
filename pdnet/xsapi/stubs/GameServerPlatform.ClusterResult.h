#pragma once
#include "GameServerPlatform/ClusterResult.g.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    struct ClusterResult : ClusterResultT<ClusterResult>
    {
        ClusterResult() = default;

        Windows::Foundation::TimeSpan PollInterval();
        Microsoft::Xbox::Services::GameServerPlatform::GameServerFulfillmentState FulfillmentState();
        hstring HostName();
        hstring Region();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerPortMapping> PortMappings();
        hstring SecureDeviceAddress();
    };
}
