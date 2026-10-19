#include "xsapi_pch.h"
#include "XboxLiveLogCallEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel XboxLiveLogCallEventArgs::Level()
    {
        PDNET_XSAPI_STUB("XboxLiveLogCallEventArgs::Level");
    }
    hstring XboxLiveLogCallEventArgs::Category()
    {
        PDNET_XSAPI_STUB("XboxLiveLogCallEventArgs::Category");
    }
    hstring XboxLiveLogCallEventArgs::Message()
    {
        PDNET_XSAPI_STUB("XboxLiveLogCallEventArgs::Message");
    }
}
