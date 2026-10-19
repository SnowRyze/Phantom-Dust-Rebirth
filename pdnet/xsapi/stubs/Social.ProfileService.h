#pragma once
#include "Social/ProfileService.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct ProfileService : ProfileServiceT<ProfileService>
    {
        ProfileService() = default;

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxUserProfile> GetUserProfileAsync(hstring xboxUserId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile>> GetUserProfilesAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile>> GetUserProfilesForSocialGroupAsync(hstring socialGroup);
    };
}
