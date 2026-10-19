#include "xsapi_pch.h"
#include "Presence.DevicePresenceChangeSubscription.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState DevicePresenceChangeSubscription::State()
    {
        PDNET_XSAPI_STUB("DevicePresenceChangeSubscription::State");
    }
    hstring DevicePresenceChangeSubscription::ResourceUri()
    {
        PDNET_XSAPI_STUB("DevicePresenceChangeSubscription::ResourceUri");
    }
    uint32_t DevicePresenceChangeSubscription::SubscriptionId()
    {
        PDNET_XSAPI_STUB("DevicePresenceChangeSubscription::SubscriptionId");
    }
    hstring DevicePresenceChangeSubscription::XboxUserId()
    {
        PDNET_XSAPI_STUB("DevicePresenceChangeSubscription::XboxUserId");
    }
}
