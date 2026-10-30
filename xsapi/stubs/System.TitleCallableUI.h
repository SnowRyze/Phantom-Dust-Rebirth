#pragma once
#include "System/TitleCallableUI.g.h"


namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct TitleCallableUI : TitleCallableUIT<TitleCallableUI>
    {
        TitleCallableUI() = default;

        static Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> ShowPlayerPickerUI(hstring promptDisplayText, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<hstring> preselectedXboxUserIds, uint32_t minSelectionCount, uint32_t maxSelectionCount);
        static Windows::Foundation::IAsyncAction ShowGameInviteUIAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, hstring contextStringId);
        static Windows::Foundation::IAsyncAction ShowProfileCardUIAsync(hstring targetXboxUserId);
        static Windows::Foundation::IAsyncAction ShowChangeFriendRelationshipUIAsync(hstring targetXboxUserId);
        static Windows::Foundation::IAsyncAction ShowTitleAchievementsUIAsync(uint32_t titleId);
        static bool CheckGamingPrivilegeSilently(Microsoft::Xbox::Services::System::GamingPrivilege const& privilege);
        static Windows::Foundation::IAsyncOperation<bool> CheckGamingPrivilegeWithUI(Microsoft::Xbox::Services::System::GamingPrivilege privilege, hstring friendlyMessage);
        static Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> ShowPlayerPickerUIForUser(hstring promptDisplayText, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<hstring> preselectedXboxUserIds, uint32_t minSelectionCount, uint32_t maxSelectionCount, Windows::System::User user);
        static Windows::Foundation::IAsyncAction ShowGameInviteUIForUserAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, hstring contextStringId, Windows::System::User user);
        static Windows::Foundation::IAsyncAction ShowProfileCardUIForUserAsync(hstring targetXboxUserId, Windows::System::User user);
        static Windows::Foundation::IAsyncAction ShowChangeFriendRelationshipUIForUserAsync(hstring targetXboxUserId, Windows::System::User user);
        static Windows::Foundation::IAsyncAction ShowTitleAchievementsUIForUserAsync(uint32_t titleId, Windows::System::User user);
        static bool CheckGamingPrivilegeSilentlyForUser(Microsoft::Xbox::Services::System::GamingPrivilege const& privilege, Windows::System::User const& user);
        static Windows::Foundation::IAsyncOperation<bool> CheckGamingPrivilegeWithUIForUser(Microsoft::Xbox::Services::System::GamingPrivilege privilege, hstring friendlyMessage, Windows::System::User user);
    };
}
namespace winrt::Microsoft::Xbox::Services::System::factory_implementation
{
    struct TitleCallableUI : TitleCallableUIT<TitleCallableUI, implementation::TitleCallableUI>
    {
    };
}
