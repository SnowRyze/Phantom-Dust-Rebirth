#pragma once
#include "System/SignInResult.g.h"

namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct SignInResult : SignInResultT<SignInResult>
    {
        SignInResult() = default;
        explicit SignInResult(Microsoft::Xbox::Services::System::SignInStatus status) : m_status(status) {}

        Microsoft::Xbox::Services::System::SignInStatus Status() { return m_status; }

    private:
        Microsoft::Xbox::Services::System::SignInStatus m_status = Microsoft::Xbox::Services::System::SignInStatus::Success;
    };
}
