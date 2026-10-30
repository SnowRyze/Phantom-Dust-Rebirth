#pragma once
#include "RealTimeActivity/RealTimeActivitySubscriptionErrorEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::RealTimeActivity::implementation
{
    struct RealTimeActivitySubscriptionErrorEventArgs : RealTimeActivitySubscriptionErrorEventArgsT<RealTimeActivitySubscriptionErrorEventArgs>
    {
        RealTimeActivitySubscriptionErrorEventArgs() = default;

        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState State();
        uint32_t SubscriptionId();
        hstring ResourceUri();
        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionError SubscriptionError();
        hstring ErrorMessage();
    };
}
