#pragma once
#include "Achievements/AchievementTimeWindow.g.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    struct AchievementTimeWindow : AchievementTimeWindowT<AchievementTimeWindow>
    {
        AchievementTimeWindow() = default;

        Windows::Foundation::DateTime StartDate();
        Windows::Foundation::DateTime EndDate();
    };
}
