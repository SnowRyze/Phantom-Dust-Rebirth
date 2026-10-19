#include "xsapi_pch.h"
#include "Achievements.AchievementReward.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    hstring AchievementReward::Name()
    {
        PDNET_XSAPI_STUB("AchievementReward::Name");
    }
    hstring AchievementReward::Description()
    {
        PDNET_XSAPI_STUB("AchievementReward::Description");
    }
    hstring AchievementReward::Data()
    {
        PDNET_XSAPI_STUB("AchievementReward::Data");
    }
    Microsoft::Xbox::Services::Achievements::AchievementRewardType AchievementReward::RewardType()
    {
        PDNET_XSAPI_STUB("AchievementReward::RewardType");
    }
    Windows::Foundation::PropertyType AchievementReward::ValuePropertyType()
    {
        PDNET_XSAPI_STUB("AchievementReward::ValuePropertyType");
    }
    Microsoft::Xbox::Services::Achievements::AchievementMediaAsset AchievementReward::MediaAsset()
    {
        PDNET_XSAPI_STUB("AchievementReward::MediaAsset");
    }
}
