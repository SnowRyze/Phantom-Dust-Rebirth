#pragma once
#include "XboxLiveWnsEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveWnsEventArgs : XboxLiveWnsEventArgsT<XboxLiveWnsEventArgs>
    {
        XboxLiveWnsEventArgs() = default;

        hstring XboxUserId();
        hstring NotificationType();
    };
}
