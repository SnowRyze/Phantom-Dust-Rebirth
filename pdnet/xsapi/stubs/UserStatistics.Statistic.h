#pragma once
#include "UserStatistics/Statistic.g.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct Statistic : StatisticT<Statistic>
    {
        Statistic() = default;

        hstring StatisticName();
        Windows::Foundation::PropertyType StatisticType();
        hstring Value();
    };
}
