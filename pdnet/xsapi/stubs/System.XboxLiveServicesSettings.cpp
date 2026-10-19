#include "xsapi_pch.h"
#include "System.XboxLiveServicesSettings.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    Microsoft::Xbox::Services::System::XboxLiveServicesSettings XboxLiveServicesSettings::SingletonInstance()
    {
        PDNET_XSAPI_STUB("XboxLiveServicesSettings::SingletonInstance");
    }
    winrt::event_token XboxLiveServicesSettings::LogCallRouted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxLiveLogCallEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("XboxLiveServicesSettings::LogCallRouted");
    }
    void XboxLiveServicesSettings::LogCallRouted(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("XboxLiveServicesSettings::LogCallRouted");
    }
    Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel XboxLiveServicesSettings::DiagnosticsTraceLevel()
    {
        PDNET_XSAPI_STUB("XboxLiveServicesSettings::DiagnosticsTraceLevel");
    }
    void XboxLiveServicesSettings::DiagnosticsTraceLevel(Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveServicesSettings::DiagnosticsTraceLevel");
    }
    winrt::event_token XboxLiveServicesSettings::WnsEventRecevied(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxLiveWnsEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("XboxLiveServicesSettings::WnsEventRecevied");
    }
    void XboxLiveServicesSettings::WnsEventRecevied(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("XboxLiveServicesSettings::WnsEventRecevied");
    }
}
