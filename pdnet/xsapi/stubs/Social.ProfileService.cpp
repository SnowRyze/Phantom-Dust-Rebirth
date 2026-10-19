#include "xsapi_pch.h"
#include "Social.ProfileService.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxUserProfile> ProfileService::GetUserProfileAsync(hstring xboxUserId)
    {
        PDNET_XSAPI_STUB("ProfileService::GetUserProfileAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile>> ProfileService::GetUserProfilesAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds)
    {
        PDNET_XSAPI_STUB("ProfileService::GetUserProfilesAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile>> ProfileService::GetUserProfilesForSocialGroupAsync(hstring socialGroup)
    {
        PDNET_XSAPI_STUB("ProfileService::GetUserProfilesForSocialGroupAsync");
    }
}
