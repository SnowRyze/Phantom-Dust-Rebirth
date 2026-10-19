#include "xsapi_pch.h"
#include "Achievements.AchievementProgression.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementRequirement> AchievementProgression::Requirements()
    {
        PDNET_XSAPI_STUB("AchievementProgression::Requirements");
    }
    Windows::Foundation::DateTime AchievementProgression::TimeUnlocked()
    {
        PDNET_XSAPI_STUB("AchievementProgression::TimeUnlocked");
    }
}
