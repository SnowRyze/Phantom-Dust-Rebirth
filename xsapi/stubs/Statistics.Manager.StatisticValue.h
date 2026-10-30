#pragma once
#include "Statistics/Manager/StatisticValue.g.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    struct StatisticValue : StatisticValueT<StatisticValue>
    {
        StatisticValue() = default;

        hstring Name();
        double AsNumber();
        int64_t AsInteger();
        hstring AsString();
        Microsoft::Xbox::Services::Statistics::Manager::StatisticDataType DataType();
    };
}
