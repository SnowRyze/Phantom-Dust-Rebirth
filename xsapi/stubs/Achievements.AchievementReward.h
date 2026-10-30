#pragma once
#include "Achievements/AchievementReward.g.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    struct AchievementReward : AchievementRewardT<AchievementReward>
    {
        AchievementReward() = default;

        hstring Name();
        hstring Description();
        hstring Data();
        Microsoft::Xbox::Services::Achievements::AchievementRewardType RewardType();
        Windows::Foundation::PropertyType ValuePropertyType();
        Microsoft::Xbox::Services::Achievements::AchievementMediaAsset MediaAsset();
    };
}
