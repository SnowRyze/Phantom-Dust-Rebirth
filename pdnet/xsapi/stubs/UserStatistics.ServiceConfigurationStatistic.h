#pragma once
#include "UserStatistics/ServiceConfigurationStatistic.g.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct ServiceConfigurationStatistic : ServiceConfigurationStatisticT<ServiceConfigurationStatistic>
    {
        ServiceConfigurationStatistic() = default;

        hstring ServiceConfigurationId();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::Statistic> Statistics();
    };
}
