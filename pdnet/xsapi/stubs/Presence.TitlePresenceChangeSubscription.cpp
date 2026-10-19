#include "xsapi_pch.h"
#include "Presence.TitlePresenceChangeSubscription.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState TitlePresenceChangeSubscription::State()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeSubscription::State");
    }
    hstring TitlePresenceChangeSubscription::ResourceUri()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeSubscription::ResourceUri");
    }
    uint32_t TitlePresenceChangeSubscription::SubscriptionId()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeSubscription::SubscriptionId");
    }
    hstring TitlePresenceChangeSubscription::XboxUserId()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeSubscription::XboxUserId");
    }
    uint32_t TitlePresenceChangeSubscription::TitleId()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeSubscription::TitleId");
    }
}
