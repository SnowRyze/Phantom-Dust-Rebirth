#pragma once
#include "Achievements/AchievementsResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    struct AchievementsResult : AchievementsResultT<AchievementsResult>
    {
        AchievementsResult() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::Achievement> Items();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Achievements::AchievementsResult> GetNextAsync(uint32_t maxItems);
        bool HasNext();
    };
}
