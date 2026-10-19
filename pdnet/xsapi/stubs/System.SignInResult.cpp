#include "xsapi_pch.h"
#include "System.SignInResult.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    Microsoft::Xbox::Services::System::SignInStatus SignInResult::Status()
    {
        PDNET_XSAPI_STUB("SignInResult::Status");
    }
}
