#include "xsapi_pch.h"
#include "System.XboxLiveUser.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    XboxLiveUser::XboxLiveUser(Windows::System::User const& systemUser)
    {
        PDNET_XSAPI_STUB("XboxLiveUser::XboxLiveUser");
    }
    winrt::event_token XboxLiveUser::SignOutCompleted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::System::SignOutCompletedEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("XboxLiveUser::SignOutCompleted");
    }
    void XboxLiveUser::SignOutCompleted(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("XboxLiveUser::SignOutCompleted");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::SignInResult> XboxLiveUser::SignInAsync(Windows::Foundation::IInspectable coreDispatcher)
    {
        PDNET_XSAPI_STUB("XboxLiveUser::SignInAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::SignInResult> XboxLiveUser::SignInSilentlyAsync(Windows::Foundation::IInspectable coreDispatcher)
    {
        PDNET_XSAPI_STUB("XboxLiveUser::SignInSilentlyAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> XboxLiveUser::GetTokenAndSignatureAsync(hstring httpMethod, hstring url, hstring headers)
    {
        PDNET_XSAPI_STUB("XboxLiveUser::GetTokenAndSignatureAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> XboxLiveUser::GetTokenAndSignatureAsync(hstring httpMethod, hstring url, hstring headers, hstring body)
    {
        PDNET_XSAPI_STUB("XboxLiveUser::GetTokenAndSignatureAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> XboxLiveUser::GetTokenAndSignatureArrayAsync(hstring httpMethod, hstring url, hstring headers, array_view<uint8_t const> requestBodyArray)
    {
        PDNET_XSAPI_STUB("XboxLiveUser::GetTokenAndSignatureArrayAsync");
    }
    hstring XboxLiveUser::XboxUserId()
    {
        PDNET_XSAPI_STUB("XboxLiveUser::XboxUserId");
    }
    hstring XboxLiveUser::Gamertag()
    {
        PDNET_XSAPI_STUB("XboxLiveUser::Gamertag");
    }
    hstring XboxLiveUser::AgeGroup()
    {
        PDNET_XSAPI_STUB("XboxLiveUser::AgeGroup");
    }
    hstring XboxLiveUser::Privileges()
    {
        PDNET_XSAPI_STUB("XboxLiveUser::Privileges");
    }
    bool XboxLiveUser::IsSignedIn()
    {
        PDNET_XSAPI_STUB("XboxLiveUser::IsSignedIn");
    }
    hstring XboxLiveUser::WebAccountId()
    {
        PDNET_XSAPI_STUB("XboxLiveUser::WebAccountId");
    }
    Windows::System::User XboxLiveUser::WindowsSystemUser()
    {
        PDNET_XSAPI_STUB("XboxLiveUser::WindowsSystemUser");
    }
}
