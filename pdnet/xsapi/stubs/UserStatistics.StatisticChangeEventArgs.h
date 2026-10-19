#pragma once
#include "UserStatistics/StatisticChangeEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct StatisticChangeEventArgs : StatisticChangeEventArgsT<StatisticChangeEventArgs>
    {
        StatisticChangeEventArgs() = default;

        hstring XboxUserId();
        hstring ServiceConfigurationId();
        Microsoft::Xbox::Services::UserStatistics::Statistic LatestStatistic();
    };
}
