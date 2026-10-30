#pragma once
#include "Presence/TitlePresenceChangeSubscription.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct TitlePresenceChangeSubscription : TitlePresenceChangeSubscriptionT<TitlePresenceChangeSubscription>
    {
        TitlePresenceChangeSubscription() = default;

        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState State();
        hstring ResourceUri();
        uint32_t SubscriptionId();
        hstring XboxUserId();
        uint32_t TitleId();
    };
}
