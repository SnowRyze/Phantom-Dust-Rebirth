#pragma once
#include "System/VerifyStringResult.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct VerifyStringResult : VerifyStringResultT<VerifyStringResult>
    {
        VerifyStringResult() = default;

        Microsoft::Xbox::Services::System::VerifyStringResultCode ResultCode();
        hstring FirstOffendingSubstring();
    };
}
