#pragma once
#include "System/XboxLiveServicesSettings.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct XboxLiveServicesSettings : XboxLiveServicesSettingsT<XboxLiveServicesSettings>
    {
        XboxLiveServicesSettings() = default;

        static Microsoft::Xbox::Services::System::XboxLiveServicesSettings SingletonInstance();
        winrt::event_token LogCallRouted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxLiveLogCallEventArgs> const& __param0);
        void LogCallRouted(winrt::event_token const& __param0) noexcept;
        Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel DiagnosticsTraceLevel();
        void DiagnosticsTraceLevel(Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel const& value);
        winrt::event_token WnsEventRecevied(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxLiveWnsEventArgs> const& __param0);
        void WnsEventRecevied(winrt::event_token const& __param0) noexcept;
    };
}
namespace winrt::Microsoft::Xbox::Services::System::factory_implementation
{
    struct XboxLiveServicesSettings : XboxLiveServicesSettingsT<XboxLiveServicesSettings, implementation::XboxLiveServicesSettings>
    {
    };
}
