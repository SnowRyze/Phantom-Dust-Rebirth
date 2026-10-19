#pragma once
#include "System/SignInResult.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct SignInResult : SignInResultT<SignInResult>
    {
        SignInResult() = default;

        Microsoft::Xbox::Services::System::SignInStatus Status();
    };
}
