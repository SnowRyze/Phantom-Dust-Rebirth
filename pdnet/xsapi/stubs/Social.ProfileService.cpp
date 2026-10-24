// pdnet-hand: profiles carry only the player's id and name (the scoreboard asks for the names of its rows).
#include "xsapi_pch.h"
#include "Social.ProfileService.h"
#include "Social.XboxUserProfile.h"

namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    namespace
    {
        Microsoft::Xbox::Services::Social::XboxUserProfile Profile(hstring const& xuid)
        {
            std::wstring name = ::pdnet::xsapi::NameOf(std::wstring{ xuid });
            return winrt::make<XboxUserProfile>(xuid, hstring{ name.empty() ? L"Player" : name });
        }
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxUserProfile> ProfileService::GetUserProfileAsync(hstring xboxUserId)
    {
        co_return Profile(xboxUserId);
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile>> ProfileService::GetUserProfilesAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds)
    {
        auto profiles = winrt::single_threaded_vector<Microsoft::Xbox::Services::Social::XboxUserProfile>();
        if (xboxUserIds)
            for (auto const& xuid : xboxUserIds) profiles.Append(Profile(xuid));
        co_return profiles.GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile>> ProfileService::GetUserProfilesForSocialGroupAsync(hstring socialGroup)
    {
        PDNET_XSAPI_STUB_DEFAULT("ProfileService::GetUserProfilesForSocialGroupAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Social::XboxUserProfile>().GetView();
    }
}
