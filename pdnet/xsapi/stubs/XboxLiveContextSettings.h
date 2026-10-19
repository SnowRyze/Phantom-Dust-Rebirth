#pragma once
#include "XboxLiveContextSettings.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveContextSettings : XboxLiveContextSettingsT<XboxLiveContextSettings>
    {
        XboxLiveContextSettings() = default;

        winrt::event_token ServiceCallRouted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs> const& __param0);
        void ServiceCallRouted(winrt::event_token const& __param0) noexcept;
        bool EnableServiceCallRoutedEvents();
        void EnableServiceCallRoutedEvents(bool value);
        Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel DiagnosticsTraceLevel();
        void DiagnosticsTraceLevel(Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel const& value);
        Windows::Foundation::TimeSpan HttpTimeout();
        void HttpTimeout(Windows::Foundation::TimeSpan const& value);
        Windows::Foundation::TimeSpan LongHttpTimeout();
        void LongHttpTimeout(Windows::Foundation::TimeSpan const& value);
        Windows::Foundation::TimeSpan HttpRetryDelay();
        void HttpRetryDelay(Windows::Foundation::TimeSpan const& value);
        Windows::Foundation::TimeSpan HttpTimeoutWindow();
        void HttpTimeoutWindow(Windows::Foundation::TimeSpan const& value);
        Windows::Foundation::TimeSpan WebsocketTimeoutWindow();
        void WebsocketTimeoutWindow(Windows::Foundation::TimeSpan const& value);
        bool UseCoreDispatcherForEventRouting();
        void UseCoreDispatcherForEventRouting(bool value);
        void DisableAssertsForXboxLiveThrottlingInDevSandboxes(Microsoft::Xbox::Services::XboxLiveContextThrottleSetting const& setting);
        void DisableAssertsForMaximumNumberOfWebsocketsActivated(Microsoft::Xbox::Services::XboxLiveContextRecommendedSetting const& setting);
    };
}
namespace winrt::Microsoft::Xbox::Services::factory_implementation
{
    struct XboxLiveContextSettings : XboxLiveContextSettingsT<XboxLiveContextSettings, implementation::XboxLiveContextSettings>
    {
    };
}
