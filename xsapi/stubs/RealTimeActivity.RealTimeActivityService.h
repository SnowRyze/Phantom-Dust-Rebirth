#pragma once
#include "RealTimeActivity/RealTimeActivityService.g.h"


namespace winrt::Microsoft::Xbox::Services::RealTimeActivity::implementation
{
    struct RealTimeActivityService : RealTimeActivityServiceT<RealTimeActivityService>
    {
        RealTimeActivityService() = default;

        void Activate();
        void Deactivate();
        winrt::event_token RealTimeActivityConnectionStateChange(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityConnectionState> const& __param0);
        void RealTimeActivityConnectionStateChange(winrt::event_token const& __param0) noexcept;
        winrt::event_token RealTimeActivitySubscriptionError(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionErrorEventArgs> const& __param0);
        void RealTimeActivitySubscriptionError(winrt::event_token const& __param0) noexcept;
        winrt::event_token RealTimeActivityResync(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityResyncEventArgs> const& __param0);
        void RealTimeActivityResync(winrt::event_token const& __param0) noexcept;
    };
}
