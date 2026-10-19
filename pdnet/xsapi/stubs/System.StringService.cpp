#include "xsapi_pch.h"
#include "System.StringService.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::VerifyStringResult> StringService::VerifyStringAsync(hstring stringToVerify)
    {
        PDNET_XSAPI_STUB("StringService::VerifyStringAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::System::VerifyStringResult>> StringService::VerifyStringsAsync(Windows::Foundation::Collections::IVectorView<hstring> stringsToVerify)
    {
        PDNET_XSAPI_STUB("StringService::VerifyStringsAsync");
    }
}
