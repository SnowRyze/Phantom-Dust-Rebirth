#include "xsapi_pch.h"
#include "UserStatistics.StatisticChangeSubscription.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState StatisticChangeSubscription::State()
    {
        PDNET_XSAPI_STUB("StatisticChangeSubscription::State");
    }
    hstring StatisticChangeSubscription::ResourceUri()
    {
        PDNET_XSAPI_STUB("StatisticChangeSubscription::ResourceUri");
    }
    uint32_t StatisticChangeSubscription::SubscriptionId()
    {
        PDNET_XSAPI_STUB("StatisticChangeSubscription::SubscriptionId");
    }
    hstring StatisticChangeSubscription::XboxUserId()
    {
        PDNET_XSAPI_STUB("StatisticChangeSubscription::XboxUserId");
    }
    hstring StatisticChangeSubscription::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("StatisticChangeSubscription::ServiceConfigurationId");
    }
    hstring StatisticChangeSubscription::StatisticName()
    {
        PDNET_XSAPI_STUB("StatisticChangeSubscription::StatisticName");
    }
}
