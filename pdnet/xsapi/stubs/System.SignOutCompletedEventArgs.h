#pragma once
#include "System/SignOutCompletedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct SignOutCompletedEventArgs : SignOutCompletedEventArgsT<SignOutCompletedEventArgs>
    {
        SignOutCompletedEventArgs() = default;

        Microsoft::Xbox::Services::System::XboxLiveUser User();
    };
}
