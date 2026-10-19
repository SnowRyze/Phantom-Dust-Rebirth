#pragma once
#include "Achievements/AchievementProgression.g.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    struct AchievementProgression : AchievementProgressionT<AchievementProgression>
    {
        AchievementProgression() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementRequirement> Requirements();
        Windows::Foundation::DateTime TimeUnlocked();
    };
}
