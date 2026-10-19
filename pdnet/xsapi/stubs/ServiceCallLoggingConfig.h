#pragma once
#include "ServiceCallLoggingConfig.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct ServiceCallLoggingConfig : ServiceCallLoggingConfigT<ServiceCallLoggingConfig>
    {
        ServiceCallLoggingConfig() = default;

        static Microsoft::Xbox::Services::ServiceCallLoggingConfig SingletonInstance();
        void Enable();
        void Disable();
        void RegisterForProtocolActivation();
    };
}
namespace winrt::Microsoft::Xbox::Services::factory_implementation
{
    struct ServiceCallLoggingConfig : ServiceCallLoggingConfigT<ServiceCallLoggingConfig, implementation::ServiceCallLoggingConfig>
    {
    };
}
