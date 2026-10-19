#include "xsapi_pch.h"
#include "ServiceCallLoggingConfig.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    Microsoft::Xbox::Services::ServiceCallLoggingConfig ServiceCallLoggingConfig::SingletonInstance()
    {
        PDNET_XSAPI_STUB("ServiceCallLoggingConfig::SingletonInstance");
    }
    void ServiceCallLoggingConfig::Enable()
    {
        PDNET_XSAPI_STUB("ServiceCallLoggingConfig::Enable");
    }
    void ServiceCallLoggingConfig::Disable()
    {
        PDNET_XSAPI_STUB("ServiceCallLoggingConfig::Disable");
    }
    void ServiceCallLoggingConfig::RegisterForProtocolActivation()
    {
        PDNET_XSAPI_STUB("ServiceCallLoggingConfig::RegisterForProtocolActivation");
    }
}
