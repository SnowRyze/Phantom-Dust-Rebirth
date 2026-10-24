#pragma once
#include "UserStatistics/ServiceConfigurationStatistic.g.h"

// pdnet-hand: one service configuration's statistics.
namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct ServiceConfigurationStatistic : ServiceConfigurationStatisticT<ServiceConfigurationStatistic>
    {
        ServiceConfigurationStatistic() = default;
        ServiceConfigurationStatistic(hstring scid, std::vector<Microsoft::Xbox::Services::UserStatistics::Statistic> statistics)
            : m_scid(std::move(scid)), m_statistics(std::move(statistics)) {}

        hstring ServiceConfigurationId() { return m_scid; }
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::Statistic> Statistics()
        {
            return winrt::single_threaded_vector(std::vector<Microsoft::Xbox::Services::UserStatistics::Statistic>(m_statistics)).GetView();
        }

    private:
        hstring m_scid;
        std::vector<Microsoft::Xbox::Services::UserStatistics::Statistic> m_statistics;
    };
}
