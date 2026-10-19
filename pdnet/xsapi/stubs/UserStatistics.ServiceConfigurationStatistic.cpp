#include "xsapi_pch.h"
#include "UserStatistics.ServiceConfigurationStatistic.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    hstring ServiceConfigurationStatistic::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("ServiceConfigurationStatistic::ServiceConfigurationId");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::Statistic> ServiceConfigurationStatistic::Statistics()
    {
        PDNET_XSAPI_STUB("ServiceConfigurationStatistic::Statistics");
    }
}
