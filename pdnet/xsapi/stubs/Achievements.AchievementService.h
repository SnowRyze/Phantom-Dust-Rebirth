#pragma once
#include "Achievements/AchievementService.g.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    struct AchievementService : AchievementServiceT<AchievementService>
    {
        AchievementService() = default;

        Windows::Foundation::IAsyncAction UpdateAchievementAsync(hstring xboxUserId, hstring achievementId, uint32_t percentComplete);
        Windows::Foundation::IAsyncAction UpdateAchievementAsync(hstring xboxUserId, uint32_t titleId, hstring serviceConfigurationId, hstring achievementId, uint32_t percentComplete);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Achievements::AchievementsResult> GetAchievementsForTitleIdAsync(hstring xboxUserId, uint32_t titleId, Microsoft::Xbox::Services::Achievements::AchievementType type, bool unlockedOnly, Microsoft::Xbox::Services::Achievements::AchievementOrderBy orderBy, uint32_t skipItems, uint32_t maxItems);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Achievements::Achievement> GetAchievementAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring achievementId);
    };
}
