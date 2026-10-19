#include "xsapi_pch.h"
#include "RealTimeActivity.RealTimeActivitySubscriptionErrorEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::RealTimeActivity::implementation
{
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState RealTimeActivitySubscriptionErrorEventArgs::State()
    {
        PDNET_XSAPI_STUB("RealTimeActivitySubscriptionErrorEventArgs::State");
    }
    uint32_t RealTimeActivitySubscriptionErrorEventArgs::SubscriptionId()
    {
        PDNET_XSAPI_STUB("RealTimeActivitySubscriptionErrorEventArgs::SubscriptionId");
    }
    hstring RealTimeActivitySubscriptionErrorEventArgs::ResourceUri()
    {
        PDNET_XSAPI_STUB("RealTimeActivitySubscriptionErrorEventArgs::ResourceUri");
    }
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionError RealTimeActivitySubscriptionErrorEventArgs::SubscriptionError()
    {
        PDNET_XSAPI_STUB("RealTimeActivitySubscriptionErrorEventArgs::SubscriptionError");
    }
    hstring RealTimeActivitySubscriptionErrorEventArgs::ErrorMessage()
    {
        PDNET_XSAPI_STUB("RealTimeActivitySubscriptionErrorEventArgs::ErrorMessage");
    }
}
