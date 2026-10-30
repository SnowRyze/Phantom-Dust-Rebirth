#pragma once
#include "UserStatistics/Statistic.g.h"

// pdnet-hand: one statistic value (the scoreboard's win counts, see UserStatistics.UserStatisticsService.cpp).
namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct Statistic : StatisticT<Statistic>
    {
        Statistic() = default;
        Statistic(hstring name, hstring value) : m_name(std::move(name)), m_value(std::move(value)) {}

        hstring StatisticName() { return m_name; }
        Windows::Foundation::PropertyType StatisticType() { return Windows::Foundation::PropertyType::Int64; }
        hstring Value() { return m_value; }

    private:
        hstring m_name, m_value;
    };
}
