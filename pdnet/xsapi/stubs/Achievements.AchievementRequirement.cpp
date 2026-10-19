#include "xsapi_pch.h"
#include "Achievements.AchievementRequirement.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    hstring AchievementRequirement::Id()
    {
        PDNET_XSAPI_STUB("AchievementRequirement::Id");
    }
    hstring AchievementRequirement::CurrentProgressValue()
    {
        PDNET_XSAPI_STUB("AchievementRequirement::CurrentProgressValue");
    }
    hstring AchievementRequirement::TargetProgressValue()
    {
        PDNET_XSAPI_STUB("AchievementRequirement::TargetProgressValue");
    }
}
