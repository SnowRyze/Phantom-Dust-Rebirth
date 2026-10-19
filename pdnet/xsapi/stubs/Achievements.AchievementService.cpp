#include "xsapi_pch.h"
#include "Achievements.AchievementService.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    Windows::Foundation::IAsyncAction AchievementService::UpdateAchievementAsync(hstring xboxUserId, hstring achievementId, uint32_t percentComplete)
    {
        PDNET_XSAPI_STUB("AchievementService::UpdateAchievementAsync");
    }
    Windows::Foundation::IAsyncAction AchievementService::UpdateAchievementAsync(hstring xboxUserId, uint32_t titleId, hstring serviceConfigurationId, hstring achievementId, uint32_t percentComplete)
    {
        PDNET_XSAPI_STUB("AchievementService::UpdateAchievementAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Achievements::AchievementsResult> AchievementService::GetAchievementsForTitleIdAsync(hstring xboxUserId, uint32_t titleId, Microsoft::Xbox::Services::Achievements::AchievementType type, bool unlockedOnly, Microsoft::Xbox::Services::Achievements::AchievementOrderBy orderBy, uint32_t skipItems, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("AchievementService::GetAchievementsForTitleIdAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Achievements::Achievement> AchievementService::GetAchievementAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring achievementId)
    {
        PDNET_XSAPI_STUB("AchievementService::GetAchievementAsync");
    }
}
