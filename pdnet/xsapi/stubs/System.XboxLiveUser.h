#pragma once
#include "System/XboxLiveUser.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct XboxLiveUser : XboxLiveUserT<XboxLiveUser>
    {
        XboxLiveUser() = default;

        XboxLiveUser(Windows::System::User const& systemUser);
        static winrt::event_token SignOutCompleted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::System::SignOutCompletedEventArgs> const& __param0);
        static void SignOutCompleted(winrt::event_token const& __param0) noexcept;
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::SignInResult> SignInAsync(Windows::Foundation::IInspectable coreDispatcher);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::SignInResult> SignInSilentlyAsync(Windows::Foundation::IInspectable coreDispatcher);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> GetTokenAndSignatureAsync(hstring httpMethod, hstring url, hstring headers);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> GetTokenAndSignatureAsync(hstring httpMethod, hstring url, hstring headers, hstring body);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> GetTokenAndSignatureArrayAsync(hstring httpMethod, hstring url, hstring headers, array_view<uint8_t const> requestBodyArray);
        hstring XboxUserId();
        hstring Gamertag();
        hstring AgeGroup();
        hstring Privileges();
        bool IsSignedIn();
        hstring WebAccountId();
        Windows::System::User WindowsSystemUser();
    };
}
namespace winrt::Microsoft::Xbox::Services::System::factory_implementation
{
    struct XboxLiveUser : XboxLiveUserT<XboxLiveUser, implementation::XboxLiveUser>
    {
    };
}
