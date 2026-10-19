#include "xsapi_pch.h"
#include "UserStatistics.StatisticChangeEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    hstring StatisticChangeEventArgs::XboxUserId()
    {
        PDNET_XSAPI_STUB("StatisticChangeEventArgs::XboxUserId");
    }
    hstring StatisticChangeEventArgs::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("StatisticChangeEventArgs::ServiceConfigurationId");
    }
    Microsoft::Xbox::Services::UserStatistics::Statistic StatisticChangeEventArgs::LatestStatistic()
    {
        PDNET_XSAPI_STUB("StatisticChangeEventArgs::LatestStatistic");
    }
}
