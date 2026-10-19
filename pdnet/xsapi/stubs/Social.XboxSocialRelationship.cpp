#include "xsapi_pch.h"
#include "Social.XboxSocialRelationship.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    hstring XboxSocialRelationship::XboxUserId()
    {
        PDNET_XSAPI_STUB("XboxSocialRelationship::XboxUserId");
    }
    bool XboxSocialRelationship::IsFavorite()
    {
        PDNET_XSAPI_STUB("XboxSocialRelationship::IsFavorite");
    }
    bool XboxSocialRelationship::IsFollowingCaller()
    {
        PDNET_XSAPI_STUB("XboxSocialRelationship::IsFollowingCaller");
    }
    Windows::Foundation::Collections::IVectorView<hstring> XboxSocialRelationship::SocialNetworks()
    {
        PDNET_XSAPI_STUB("XboxSocialRelationship::SocialNetworks");
    }
}
