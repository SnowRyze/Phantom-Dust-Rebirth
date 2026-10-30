#include "xsapi_pch.h"
#include "System.XboxLiveUser.h"
#include "System.SignInResult.h"
#include "System.GetTokenAndSignatureResult.h"

namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    namespace
    {
        winrt::event<Windows::Foundation::EventHandler<Microsoft::Xbox::Services::System::SignOutCompletedEventArgs>> g_signOutCompleted;
    }

    XboxLiveUser::XboxLiveUser(Windows::System::User const& systemUser) : m_systemUser(systemUser)
    {
    }

    winrt::event_token XboxLiveUser::SignOutCompleted(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::System::SignOutCompletedEventArgs> const& handler)
    {
        return g_signOutCompleted.add(handler);
    }

    void XboxLiveUser::SignOutCompleted(winrt::event_token const& token) noexcept
    {
        g_signOutCompleted.remove(token);
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::SignInResult> XboxLiveUser::SignIn(const wchar_t* how)
    {
        m_signedIn = true;
        ::pdnet::Log(L"xsapi-pdnet: %s -> signed in locally as '%s' (id %s)", how, Gamertag().c_str(), XboxUserId().c_str());
        co_return winrt::make<implementation::SignInResult>(SignInStatus::Success);
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::SignInResult> XboxLiveUser::SignInAsync(Windows::Foundation::IInspectable)
    {
        return SignIn(L"SignInAsync");
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::SignInResult> XboxLiveUser::SignInSilentlyAsync(Windows::Foundation::IInspectable)
    {
        return SignIn(L"SignInSilentlyAsync");
    }

    // There is no Xbox Live token to give; callers (e.g. Meld's web backend) get an empty one and should fail gracefully.
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> XboxLiveUser::NoToken(hstring url)
    {
        ::pdnet::Log(L"xsapi-pdnet: GetTokenAndSignature for %s -> empty token (no Xbox Live)", url.c_str());
        co_return winrt::make<implementation::GetTokenAndSignatureResult>(XboxUserId(), Gamertag(), AgeGroup(), Privileges(), WebAccountId());
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> XboxLiveUser::GetTokenAndSignatureAsync(hstring, hstring url, hstring)
    {
        return NoToken(url);
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> XboxLiveUser::GetTokenAndSignatureAsync(hstring, hstring url, hstring, hstring)
    {
        return NoToken(url);
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult> XboxLiveUser::GetTokenAndSignatureArrayAsync(hstring, hstring url, hstring, array_view<uint8_t const>)
    {
        return NoToken(url);
    }

    hstring XboxLiveUser::XboxUserId() { return hstring{ ::pdnet::xsapi::LocalXuid() }; }
    hstring XboxLiveUser::Gamertag() { return hstring{ ::pdnet::GetConfig().displayName }; }
    hstring XboxLiveUser::AgeGroup() { return L"Adult"; }

    // Xbox privilege ids the game may check: 254 multiplayer, 252 communications, 249 profile viewing,
    // 247 user-created content, 211 social network sharing, 188 add friend.
    hstring XboxLiveUser::Privileges() { return L"188 211 247 249 252 254"; }

    bool XboxLiveUser::IsSignedIn() { return m_signedIn; }
    hstring XboxLiveUser::WebAccountId() { return L"pdnet-local"; }
    Windows::System::User XboxLiveUser::WindowsSystemUser() { return m_systemUser; }
}
