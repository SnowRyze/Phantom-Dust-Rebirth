#pragma once
#include "Achievements/Achievement.g.h"


namespace winrt::Microsoft::Xbox::Services::Achievements::implementation
{
    struct Achievement : AchievementT<Achievement>
    {
        Achievement() = default;

        hstring Id();
        hstring ServiceConfigurationId();
        hstring Name();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementTitleAssociation> TitleAssociations();
        Microsoft::Xbox::Services::Achievements::AchievementProgressState ProgressState();
        Microsoft::Xbox::Services::Achievements::AchievementProgression Progression();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementMediaAsset> MediaAssets();
        Windows::Foundation::Collections::IVectorView<hstring> PlatformsAvailableOn();
        bool IsSecret();
        hstring UnlockedDescription();
        hstring LockedDescription();
        hstring ProductId();
        Microsoft::Xbox::Services::Achievements::AchievementType AchievementType();
        Microsoft::Xbox::Services::Achievements::AchievementParticipationType ParticipationType();
        Microsoft::Xbox::Services::Achievements::AchievementTimeWindow Available();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Achievements::AchievementReward> Rewards();
        Windows::Foundation::TimeSpan EstimatedUnlockTime();
        hstring DeepLink();
        bool IsRevoked();
    };
}
