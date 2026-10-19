#include "xsapi_pch.h"
#include "Achievements.AchievementsResult.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::Achievement> AchievementsResult::Items()
    {
        PDNET_XSAPI_STUB("AchievementsResult::Items");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Achievements::AchievementsResult> AchievementsResult::GetNextAsync(uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("AchievementsResult::GetNextAsync");
    }
    bool AchievementsResult::HasNext()
    {
        PDNET_XSAPI_STUB("AchievementsResult::HasNext");
    }
}
