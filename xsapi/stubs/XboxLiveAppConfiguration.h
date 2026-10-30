#pragma once
#include "XboxLiveAppConfiguration.g.h"

// pdnet: Phantom Dust's real identifiers (from xboxservices.config), so anything that reads them still sees them.
namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveAppConfiguration : XboxLiveAppConfigurationT<XboxLiveAppConfiguration>
    {
        XboxLiveAppConfiguration() = default;

        static Microsoft::Xbox::Services::XboxLiveAppConfiguration SingletonInstance()
        {
            static Microsoft::Xbox::Services::XboxLiveAppConfiguration instance = winrt::make<XboxLiveAppConfiguration>();
            return instance;
        }
        uint32_t TitleId() { return 267248679; }
        hstring ServiceConfigurationId() { return L"164d0100-a5f0-44b8-90ce-bf3a0fede427"; }
        hstring Environment() { return L""; }
        hstring Sandbox() { return L"RETAIL"; }
    };
}
namespace winrt::Microsoft::Xbox::Services::factory_implementation
{
    struct XboxLiveAppConfiguration : XboxLiveAppConfigurationT<XboxLiveAppConfiguration, implementation::XboxLiveAppConfiguration>
    {
    };
}
