#pragma once
#include "UserStatistics/UserStatisticsResult.g.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct UserStatisticsResult : UserStatisticsResultT<UserStatisticsResult>
    {
        UserStatisticsResult() = default;

        hstring XboxUserId();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::ServiceConfigurationStatistic> ServiceConfigurationStatistics();
    };
}
