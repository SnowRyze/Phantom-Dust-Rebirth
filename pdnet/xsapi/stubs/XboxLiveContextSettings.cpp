#include "xsapi_pch.h"
#include "XboxLiveContextSettings.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    winrt::event_token XboxLiveContextSettings::ServiceCallRouted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::ServiceCallRouted");
    }
    void XboxLiveContextSettings::ServiceCallRouted(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("XboxLiveContextSettings::ServiceCallRouted");
    }
    bool XboxLiveContextSettings::EnableServiceCallRoutedEvents()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::EnableServiceCallRoutedEvents");
    }
    void XboxLiveContextSettings::EnableServiceCallRoutedEvents(bool value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::EnableServiceCallRoutedEvents");
    }
    Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel XboxLiveContextSettings::DiagnosticsTraceLevel()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::DiagnosticsTraceLevel");
    }
    void XboxLiveContextSettings::DiagnosticsTraceLevel(Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::DiagnosticsTraceLevel");
    }
    Windows::Foundation::TimeSpan XboxLiveContextSettings::HttpTimeout()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::HttpTimeout");
    }
    void XboxLiveContextSettings::HttpTimeout(Windows::Foundation::TimeSpan const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::HttpTimeout");
    }
    Windows::Foundation::TimeSpan XboxLiveContextSettings::LongHttpTimeout()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::LongHttpTimeout");
    }
    void XboxLiveContextSettings::LongHttpTimeout(Windows::Foundation::TimeSpan const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::LongHttpTimeout");
    }
    Windows::Foundation::TimeSpan XboxLiveContextSettings::HttpRetryDelay()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::HttpRetryDelay");
    }
    void XboxLiveContextSettings::HttpRetryDelay(Windows::Foundation::TimeSpan const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::HttpRetryDelay");
    }
    Windows::Foundation::TimeSpan XboxLiveContextSettings::HttpTimeoutWindow()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::HttpTimeoutWindow");
    }
    void XboxLiveContextSettings::HttpTimeoutWindow(Windows::Foundation::TimeSpan const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::HttpTimeoutWindow");
    }
    Windows::Foundation::TimeSpan XboxLiveContextSettings::WebsocketTimeoutWindow()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::WebsocketTimeoutWindow");
    }
    void XboxLiveContextSettings::WebsocketTimeoutWindow(Windows::Foundation::TimeSpan const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::WebsocketTimeoutWindow");
    }
    bool XboxLiveContextSettings::UseCoreDispatcherForEventRouting()
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::UseCoreDispatcherForEventRouting");
    }
    void XboxLiveContextSettings::UseCoreDispatcherForEventRouting(bool value)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::UseCoreDispatcherForEventRouting");
    }
    void XboxLiveContextSettings::DisableAssertsForXboxLiveThrottlingInDevSandboxes(Microsoft::Xbox::Services::XboxLiveContextThrottleSetting const& setting)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::DisableAssertsForXboxLiveThrottlingInDevSandboxes");
    }
    void XboxLiveContextSettings::DisableAssertsForMaximumNumberOfWebsocketsActivated(Microsoft::Xbox::Services::XboxLiveContextRecommendedSetting const& setting)
    {
        PDNET_XSAPI_STUB("XboxLiveContextSettings::DisableAssertsForMaximumNumberOfWebsocketsActivated");
    }
}
