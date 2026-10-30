#pragma once
#include "UserStatistics/RequestedStatistics.g.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct RequestedStatistics : RequestedStatisticsT<RequestedStatistics>
    {
        RequestedStatistics() = default;

        RequestedStatistics(hstring const& serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> const& statistics);
        hstring ServiceConfigurationId();
        Windows::Foundation::Collections::IVectorView<hstring> Statistics();
    };
}
namespace winrt::Microsoft::Xbox::Services::UserStatistics::factory_implementation
{
    struct RequestedStatistics : RequestedStatisticsT<RequestedStatistics, implementation::RequestedStatistics>
    {
    };
}
