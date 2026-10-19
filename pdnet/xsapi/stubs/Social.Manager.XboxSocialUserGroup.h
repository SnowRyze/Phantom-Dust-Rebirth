#pragma once
#include "Social/Manager/XboxSocialUserGroup.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct XboxSocialUserGroup : XboxSocialUserGroupT<XboxSocialUserGroup>
    {
        XboxSocialUserGroup() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::XboxSocialUser> Users();
        Microsoft::Xbox::Services::Social::Manager::SocialUserGroupType SocialUserGroupType();
        Windows::Foundation::Collections::IVectorView<hstring> UsersTrackedBySocialUserGroup();
        Microsoft::Xbox::Services::System::XboxLiveUser LocalUser();
        Microsoft::Xbox::Services::Social::Manager::PresenceFilter PresenceFilterOfGroup();
        Microsoft::Xbox::Services::Social::Manager::RelationshipFilter RelationshipFilterOfGroup();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::XboxSocialUser> GetUsersFromXboxUserIds(Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds);
    };
}
