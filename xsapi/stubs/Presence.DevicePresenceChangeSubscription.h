#pragma once
#include "Presence/DevicePresenceChangeSubscription.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct DevicePresenceChangeSubscription : DevicePresenceChangeSubscriptionT<DevicePresenceChangeSubscription>
    {
        DevicePresenceChangeSubscription() = default;

        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState State();
        hstring ResourceUri();
        uint32_t SubscriptionId();
        hstring XboxUserId();
    };
}
