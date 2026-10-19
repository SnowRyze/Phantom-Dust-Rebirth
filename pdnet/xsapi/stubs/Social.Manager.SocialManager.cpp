#include "xsapi_pch.h"
#include "Social.Manager.SocialManager.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    Microsoft::Xbox::Services::Social::Manager::SocialManager SocialManager::SingletonInstance()
    {
        PDNET_XSAPI_STUB("SocialManager::SingletonInstance");
    }
    void SocialManager::AddLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Microsoft::Xbox::Services::Social::Manager::SocialManagerExtraDetailLevel const& extraDetailLevel)
    {
        PDNET_XSAPI_STUB("SocialManager::AddLocalUser");
    }
    void SocialManager::RemoveLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("SocialManager::RemoveLocalUser");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::SocialEvent> SocialManager::DoWork()
    {
        PDNET_XSAPI_STUB("SocialManager::DoWork");
    }
    Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup SocialManager::CreateSocialUserGroupFromFilters(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Microsoft::Xbox::Services::Social::Manager::PresenceFilter const& presenceFilter, Microsoft::Xbox::Services::Social::Manager::RelationshipFilter const& relationshipFilter)
    {
        PDNET_XSAPI_STUB("SocialManager::CreateSocialUserGroupFromFilters");
    }
    Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup SocialManager::CreateSocialUserGroupFromList(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIdList)
    {
        PDNET_XSAPI_STUB("SocialManager::CreateSocialUserGroupFromList");
    }
    void SocialManager::DestroySocialUserGroup(Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup const& xboxSocialUserGroup)
    {
        PDNET_XSAPI_STUB("SocialManager::DestroySocialUserGroup");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::System::XboxLiveUser> SocialManager::LocalUsers()
    {
        PDNET_XSAPI_STUB("SocialManager::LocalUsers");
    }
    void SocialManager::UpdateSocialUserGroup(Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup const& socialGroup, Windows::Foundation::Collections::IVectorView<hstring> const& users)
    {
        PDNET_XSAPI_STUB("SocialManager::UpdateSocialUserGroup");
    }
    void SocialManager::SetRichPresencePollingState(Microsoft::Xbox::Services::System::XboxLiveUser const& user, bool shouldEnablePolling)
    {
        PDNET_XSAPI_STUB("SocialManager::SetRichPresencePollingState");
    }
}
