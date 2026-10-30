#pragma once
#include "RealTimeActivity/RealTimeActivityResyncEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::RealTimeActivity::implementation
{
    struct RealTimeActivityResyncEventArgs : RealTimeActivityResyncEventArgsT<RealTimeActivityResyncEventArgs>
    {
        RealTimeActivityResyncEventArgs() = default;

    };
}
