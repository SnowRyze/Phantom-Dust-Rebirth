#pragma once
#include "Social/Manager/SocialUserGroupLoadedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct SocialUserGroupLoadedEventArgs : SocialUserGroupLoadedEventArgsT<SocialUserGroupLoadedEventArgs>
    {
        SocialUserGroupLoadedEventArgs() = default;

        Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup SocialUserGroup();
    };
}
