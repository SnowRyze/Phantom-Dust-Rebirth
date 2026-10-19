#include "xsapi_pch.h"
#include "XboxLiveAppConfiguration.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    Microsoft::Xbox::Services::XboxLiveAppConfiguration XboxLiveAppConfiguration::SingletonInstance()
    {
        PDNET_XSAPI_STUB("XboxLiveAppConfiguration::SingletonInstance");
    }
    uint32_t XboxLiveAppConfiguration::TitleId()
    {
        PDNET_XSAPI_STUB("XboxLiveAppConfiguration::TitleId");
    }
    hstring XboxLiveAppConfiguration::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("XboxLiveAppConfiguration::ServiceConfigurationId");
    }
    hstring XboxLiveAppConfiguration::Environment()
    {
        PDNET_XSAPI_STUB("XboxLiveAppConfiguration::Environment");
    }
    hstring XboxLiveAppConfiguration::Sandbox()
    {
        PDNET_XSAPI_STUB("XboxLiveAppConfiguration::Sandbox");
    }
}
