#include "xsapi_pch.h"
#include "UserStatistics.UserStatisticsResult.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    hstring UserStatisticsResult::XboxUserId()
    {
        PDNET_XSAPI_STUB("UserStatisticsResult::XboxUserId");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::ServiceConfigurationStatistic> UserStatisticsResult::ServiceConfigurationStatistics()
    {
        PDNET_XSAPI_STUB("UserStatisticsResult::ServiceConfigurationStatistics");
    }
}
