#pragma once
#include "Social/Manager/SocialManager.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct SocialManager : SocialManagerT<SocialManager>
    {
        SocialManager() = default;

        static Microsoft::Xbox::Services::Social::Manager::SocialManager SingletonInstance();
        void AddLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Microsoft::Xbox::Services::Social::Manager::SocialManagerExtraDetailLevel const& extraDetailLevel);
        void RemoveLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::SocialEvent> DoWork();
        Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup CreateSocialUserGroupFromFilters(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Microsoft::Xbox::Services::Social::Manager::PresenceFilter const& presenceFilter, Microsoft::Xbox::Services::Social::Manager::RelationshipFilter const& relationshipFilter);
        Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup CreateSocialUserGroupFromList(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIdList);
        void DestroySocialUserGroup(Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup const& xboxSocialUserGroup);
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::System::XboxLiveUser> LocalUsers();
        void UpdateSocialUserGroup(Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup const& socialGroup, Windows::Foundation::Collections::IVectorView<hstring> const& users);
        void SetRichPresencePollingState(Microsoft::Xbox::Services::System::XboxLiveUser const& user, bool shouldEnablePolling);
    };
}
namespace winrt::Microsoft::Xbox::Services::Social::Manager::factory_implementation
{
    struct SocialManager : SocialManagerT<SocialManager, implementation::SocialManager>
    {
    };
}
