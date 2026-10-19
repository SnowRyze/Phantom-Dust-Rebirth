#include "xsapi_pch.h"
#include "UserStatistics.Statistic.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    hstring Statistic::StatisticName()
    {
        PDNET_XSAPI_STUB("Statistic::StatisticName");
    }
    Windows::Foundation::PropertyType Statistic::StatisticType()
    {
        PDNET_XSAPI_STUB("Statistic::StatisticType");
    }
    hstring Statistic::Value()
    {
        PDNET_XSAPI_STUB("Statistic::Value");
    }
}
