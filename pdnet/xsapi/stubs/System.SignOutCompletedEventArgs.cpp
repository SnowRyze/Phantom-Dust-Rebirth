#include "xsapi_pch.h"
#include "System.SignOutCompletedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    Microsoft::Xbox::Services::System::XboxLiveUser SignOutCompletedEventArgs::User()
    {
        PDNET_XSAPI_STUB("SignOutCompletedEventArgs::User");
    }
}
