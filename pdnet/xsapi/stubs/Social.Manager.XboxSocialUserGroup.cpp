#include "xsapi_pch.h"
#include "Social.Manager.XboxSocialUserGroup.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::XboxSocialUser> XboxSocialUserGroup::Users()
    {
        PDNET_XSAPI_STUB("XboxSocialUserGroup::Users");
    }
    Microsoft::Xbox::Services::Social::Manager::SocialUserGroupType XboxSocialUserGroup::SocialUserGroupType()
    {
        PDNET_XSAPI_STUB("XboxSocialUserGroup::SocialUserGroupType");
    }
    Windows::Foundation::Collections::IVectorView<hstring> XboxSocialUserGroup::UsersTrackedBySocialUserGroup()
    {
        PDNET_XSAPI_STUB("XboxSocialUserGroup::UsersTrackedBySocialUserGroup");
    }
    Microsoft::Xbox::Services::System::XboxLiveUser XboxSocialUserGroup::LocalUser()
    {
        PDNET_XSAPI_STUB("XboxSocialUserGroup::LocalUser");
    }
    Microsoft::Xbox::Services::Social::Manager::PresenceFilter XboxSocialUserGroup::PresenceFilterOfGroup()
    {
        PDNET_XSAPI_STUB("XboxSocialUserGroup::PresenceFilterOfGroup");
    }
    Microsoft::Xbox::Services::Social::Manager::RelationshipFilter XboxSocialUserGroup::RelationshipFilterOfGroup()
    {
        PDNET_XSAPI_STUB("XboxSocialUserGroup::RelationshipFilterOfGroup");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::XboxSocialUser> XboxSocialUserGroup::GetUsersFromXboxUserIds(Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds)
    {
        PDNET_XSAPI_STUB("XboxSocialUserGroup::GetUsersFromXboxUserIds");
    }
}
