#pragma once
#include "XboxLiveContextSettings.g.h"

// pdnet: plain property bag. No service calls are routed (there is no Xbox Live), so ServiceCallRouted never fires.
namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveContextSettings : XboxLiveContextSettingsT<XboxLiveContextSettings>
    {
        XboxLiveContextSettings() = default;

        winrt::event_token ServiceCallRouted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs> const& handler) { return m_routed.add(handler); }
        void ServiceCallRouted(winrt::event_token const& token) noexcept { m_routed.remove(token); }
        bool EnableServiceCallRoutedEvents() { return m_routedEvents; }
        void EnableServiceCallRoutedEvents(bool value) { m_routedEvents = value; }
        Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel DiagnosticsTraceLevel() { return m_traceLevel; }
        void DiagnosticsTraceLevel(Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel const& value) { m_traceLevel = value; }
        Windows::Foundation::TimeSpan HttpTimeout() { return m_httpTimeout; }
        void HttpTimeout(Windows::Foundation::TimeSpan const& value) { m_httpTimeout = value; }
        Windows::Foundation::TimeSpan LongHttpTimeout() { return m_longHttpTimeout; }
        void LongHttpTimeout(Windows::Foundation::TimeSpan const& value) { m_longHttpTimeout = value; }
        Windows::Foundation::TimeSpan HttpRetryDelay() { return m_retryDelay; }
        void HttpRetryDelay(Windows::Foundation::TimeSpan const& value) { m_retryDelay = value; }
        Windows::Foundation::TimeSpan HttpTimeoutWindow() { return m_timeoutWindow; }
        void HttpTimeoutWindow(Windows::Foundation::TimeSpan const& value) { m_timeoutWindow = value; }
        Windows::Foundation::TimeSpan WebsocketTimeoutWindow() { return m_websocketWindow; }
        void WebsocketTimeoutWindow(Windows::Foundation::TimeSpan const& value) { m_websocketWindow = value; }
        bool UseCoreDispatcherForEventRouting() { return m_useDispatcher; }
        void UseCoreDispatcherForEventRouting(bool value) { m_useDispatcher = value; }
        void DisableAssertsForXboxLiveThrottlingInDevSandboxes(Microsoft::Xbox::Services::XboxLiveContextThrottleSetting const&) {}
        void DisableAssertsForMaximumNumberOfWebsocketsActivated(Microsoft::Xbox::Services::XboxLiveContextRecommendedSetting const&) {}

    private:
        winrt::event<Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs>> m_routed;
        bool m_routedEvents = false;
        bool m_useDispatcher = false;
        Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel m_traceLevel{};
        Windows::Foundation::TimeSpan m_httpTimeout = std::chrono::seconds(30);
        Windows::Foundation::TimeSpan m_longHttpTimeout = std::chrono::seconds(300);
        Windows::Foundation::TimeSpan m_retryDelay = std::chrono::seconds(2);
        Windows::Foundation::TimeSpan m_timeoutWindow = std::chrono::seconds(20);
        Windows::Foundation::TimeSpan m_websocketWindow = std::chrono::seconds(300);
    };
}
namespace winrt::Microsoft::Xbox::Services::factory_implementation
{
    struct XboxLiveContextSettings : XboxLiveContextSettingsT<XboxLiveContextSettings, implementation::XboxLiveContextSettings>
    {
    };
}
