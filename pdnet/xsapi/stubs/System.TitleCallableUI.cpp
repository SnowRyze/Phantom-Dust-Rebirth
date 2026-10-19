#include "xsapi_pch.h"
#include "System.TitleCallableUI.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> TitleCallableUI::ShowPlayerPickerUI(hstring promptDisplayText, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<hstring> preselectedXboxUserIds, uint32_t minSelectionCount, uint32_t maxSelectionCount)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowPlayerPickerUI");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowGameInviteUIAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, hstring contextStringId)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowGameInviteUIAsync");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowProfileCardUIAsync(hstring targetXboxUserId)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowProfileCardUIAsync");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowChangeFriendRelationshipUIAsync(hstring targetXboxUserId)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowChangeFriendRelationshipUIAsync");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowTitleAchievementsUIAsync(uint32_t titleId)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowTitleAchievementsUIAsync");
    }
    bool TitleCallableUI::CheckGamingPrivilegeSilently(Microsoft::Xbox::Services::System::GamingPrivilege const& privilege)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::CheckGamingPrivilegeSilently");
    }
    Windows::Foundation::IAsyncOperation<bool> TitleCallableUI::CheckGamingPrivilegeWithUI(Microsoft::Xbox::Services::System::GamingPrivilege privilege, hstring friendlyMessage)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::CheckGamingPrivilegeWithUI");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> TitleCallableUI::ShowPlayerPickerUIForUser(hstring promptDisplayText, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<hstring> preselectedXboxUserIds, uint32_t minSelectionCount, uint32_t maxSelectionCount, Windows::System::User user)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowPlayerPickerUIForUser");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowGameInviteUIForUserAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, hstring contextStringId, Windows::System::User user)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowGameInviteUIForUserAsync");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowProfileCardUIForUserAsync(hstring targetXboxUserId, Windows::System::User user)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowProfileCardUIForUserAsync");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowChangeFriendRelationshipUIForUserAsync(hstring targetXboxUserId, Windows::System::User user)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowChangeFriendRelationshipUIForUserAsync");
    }
    Windows::Foundation::IAsyncAction TitleCallableUI::ShowTitleAchievementsUIForUserAsync(uint32_t titleId, Windows::System::User user)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::ShowTitleAchievementsUIForUserAsync");
    }
    bool TitleCallableUI::CheckGamingPrivilegeSilentlyForUser(Microsoft::Xbox::Services::System::GamingPrivilege const& privilege, Windows::System::User const& user)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::CheckGamingPrivilegeSilentlyForUser");
    }
    Windows::Foundation::IAsyncOperation<bool> TitleCallableUI::CheckGamingPrivilegeWithUIForUser(Microsoft::Xbox::Services::System::GamingPrivilege privilege, hstring friendlyMessage, Windows::System::User user)
    {
        PDNET_XSAPI_STUB("TitleCallableUI::CheckGamingPrivilegeWithUIForUser");
    }
}
