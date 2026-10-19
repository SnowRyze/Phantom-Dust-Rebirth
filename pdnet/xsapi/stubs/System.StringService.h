#pragma once
#include "System/StringService.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct StringService : StringServiceT<StringService>
    {
        StringService() = default;

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::VerifyStringResult> VerifyStringAsync(hstring stringToVerify);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::System::VerifyStringResult>> VerifyStringsAsync(Windows::Foundation::Collections::IVectorView<hstring> stringsToVerify);
    };
}
