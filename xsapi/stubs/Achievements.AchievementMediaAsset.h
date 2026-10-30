#pragma once
#include "Achievements/AchievementMediaAsset.g.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    struct AchievementMediaAsset : AchievementMediaAssetT<AchievementMediaAsset>
    {
        AchievementMediaAsset() = default;

        hstring Name();
        Microsoft::Xbox::Services::Achievements::AchievementMediaAssetType MediaAssetType();
        hstring Url();
    };
}
