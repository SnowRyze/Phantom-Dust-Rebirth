#include "xsapi_pch.h"
#include "Social.Manager.SocialUserGroupLoadedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup SocialUserGroupLoadedEventArgs::SocialUserGroup()
    {
        PDNET_XSAPI_STUB("SocialUserGroupLoadedEventArgs::SocialUserGroup");
    }
}
