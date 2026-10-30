#pragma once
#include "UserStatistics/UserStatisticsResult.g.h"

// pdnet-hand: one player's statistics.
namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct UserStatisticsResult : UserStatisticsResultT<UserStatisticsResult>
    {
        UserStatisticsResult() = default;
        UserStatisticsResult(hstring xuid, std::vector<Microsoft::Xbox::Services::UserStatistics::ServiceConfigurationStatistic> statistics)
            : m_xuid(std::move(xuid)), m_statistics(std::move(statistics)) {}

        hstring XboxUserId() { return m_xuid; }
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::ServiceConfigurationStatistic> ServiceConfigurationStatistics()
        {
            return winrt::single_threaded_vector(std::vector<Microsoft::Xbox::Services::UserStatistics::ServiceConfigurationStatistic>(m_statistics)).GetView();
        }

    private:
        hstring m_xuid;
        std::vector<Microsoft::Xbox::Services::UserStatistics::ServiceConfigurationStatistic> m_statistics;
    };
}
