#pragma once
#include "System/GetTokenAndSignatureResult.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct GetTokenAndSignatureResult : GetTokenAndSignatureResultT<GetTokenAndSignatureResult>
    {
        GetTokenAndSignatureResult() = default;

        hstring Token();
        hstring Signature();
        hstring XboxUserId();
        hstring Gamertag();
        hstring XboxUserHash();
        hstring AgeGroup();
        hstring Privileges();
        hstring WebAccountId();
    };
}
