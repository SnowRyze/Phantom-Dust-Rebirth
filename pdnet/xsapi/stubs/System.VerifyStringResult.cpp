#include "xsapi_pch.h"
#include "System.VerifyStringResult.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    Microsoft::Xbox::Services::System::VerifyStringResultCode VerifyStringResult::ResultCode()
    {
        PDNET_XSAPI_STUB("VerifyStringResult::ResultCode");
    }
    hstring VerifyStringResult::FirstOffendingSubstring()
    {
        PDNET_XSAPI_STUB("VerifyStringResult::FirstOffendingSubstring");
    }
}
