#include "xsapi_pch.h"
#include "RealTimeActivity.RealTimeActivityService.h"


namespace winrt::Microsoft::Xbox::Services::RealTimeActivity::implementation
{
    void RealTimeActivityService::Activate()
    {
        PDNET_XSAPI_STUB("RealTimeActivityService::Activate");
    }
    void RealTimeActivityService::Deactivate()
    {
        PDNET_XSAPI_STUB("RealTimeActivityService::Deactivate");
    }
    winrt::event_token RealTimeActivityService::RealTimeActivityConnectionStateChange(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityConnectionState> const& __param0)
    {
        PDNET_XSAPI_STUB("RealTimeActivityService::RealTimeActivityConnectionStateChange");
    }
    void RealTimeActivityService::RealTimeActivityConnectionStateChange(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("RealTimeActivityService::RealTimeActivityConnectionStateChange");
    }
    winrt::event_token RealTimeActivityService::RealTimeActivitySubscriptionError(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionErrorEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("RealTimeActivityService::RealTimeActivitySubscriptionError");
    }
    void RealTimeActivityService::RealTimeActivitySubscriptionError(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("RealTimeActivityService::RealTimeActivitySubscriptionError");
    }
    winrt::event_token RealTimeActivityService::RealTimeActivityResync(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityResyncEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("RealTimeActivityService::RealTimeActivityResync");
    }
    void RealTimeActivityService::RealTimeActivityResync(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("RealTimeActivityService::RealTimeActivityResync");
    }
}
