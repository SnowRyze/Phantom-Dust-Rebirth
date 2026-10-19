#pragma once
#include "XboxLiveLogCallEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveLogCallEventArgs : XboxLiveLogCallEventArgsT<XboxLiveLogCallEventArgs>
    {
        XboxLiveLogCallEventArgs() = default;

        Microsoft::Xbox::Services::XboxServicesDiagnosticsTraceLevel Level();
        hstring Category();
        hstring Message();
    };
}
