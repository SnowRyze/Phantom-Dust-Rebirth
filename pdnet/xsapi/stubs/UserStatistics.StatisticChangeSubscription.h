#pragma once
#include "UserStatistics/StatisticChangeSubscription.g.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct StatisticChangeSubscription : StatisticChangeSubscriptionT<StatisticChangeSubscription>
    {
        StatisticChangeSubscription() = default;

        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState State();
        hstring ResourceUri();
        uint32_t SubscriptionId();
        hstring XboxUserId();
        hstring ServiceConfigurationId();
        hstring StatisticName();
    };
}
