#include "xsapi_pch.h"
#include "Achievements.Achievement.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    hstring Achievement::Id()
    {
        PDNET_XSAPI_STUB("Achievement::Id");
    }
    hstring Achievement::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("Achievement::ServiceConfigurationId");
    }
    hstring Achievement::Name()
    {
        PDNET_XSAPI_STUB("Achievement::Name");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementTitleAssociation> Achievement::TitleAssociations()
    {
        PDNET_XSAPI_STUB("Achievement::TitleAssociations");
    }
    Microsoft::Xbox::Services::Achievements::AchievementProgressState Achievement::ProgressState()
    {
        PDNET_XSAPI_STUB("Achievement::ProgressState");
    }
    Microsoft::Xbox::Services::Achievements::AchievementProgression Achievement::Progression()
    {
        PDNET_XSAPI_STUB("Achievement::Progression");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementMediaAsset> Achievement::MediaAssets()
    {
        PDNET_XSAPI_STUB("Achievement::MediaAssets");
    }
    Windows::Foundation::Collections::IVectorView<hstring> Achievement::PlatformsAvailableOn()
    {
        PDNET_XSAPI_STUB("Achievement::PlatformsAvailableOn");
    }
    bool Achievement::IsSecret()
    {
        PDNET_XSAPI_STUB("Achievement::IsSecret");
    }
    hstring Achievement::UnlockedDescription()
    {
        PDNET_XSAPI_STUB("Achievement::UnlockedDescription");
    }
    hstring Achievement::LockedDescription()
    {
        PDNET_XSAPI_STUB("Achievement::LockedDescription");
    }
    hstring Achievement::ProductId()
    {
        PDNET_XSAPI_STUB("Achievement::ProductId");
    }
    Microsoft::Xbox::Services::Achievements::AchievementType Achievement::AchievementType()
    {
        PDNET_XSAPI_STUB("Achievement::AchievementType");
    }
    Microsoft::Xbox::Services::Achievements::AchievementParticipationType Achievement::ParticipationType()
    {
        PDNET_XSAPI_STUB("Achievement::ParticipationType");
    }
    Microsoft::Xbox::Services::Achievements::AchievementTimeWindow Achievement::Available()
    {
        PDNET_XSAPI_STUB("Achievement::Available");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementReward> Achievement::Rewards()
    {
        PDNET_XSAPI_STUB("Achievement::Rewards");
    }
    Windows::Foundation::TimeSpan Achievement::EstimatedUnlockTime()
    {
        PDNET_XSAPI_STUB("Achievement::EstimatedUnlockTime");
    }
    hstring Achievement::DeepLink()
    {
        PDNET_XSAPI_STUB("Achievement::DeepLink");
    }
    bool Achievement::IsRevoked()
    {
        PDNET_XSAPI_STUB("Achievement::IsRevoked");
    }
}
