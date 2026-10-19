#include "xsapi_pch.h"
#include "UserStatistics.RequestedStatistics.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    RequestedStatistics::RequestedStatistics(hstring const& serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> const& statistics)
    {
        PDNET_XSAPI_STUB("RequestedStatistics::RequestedStatistics");
    }
    hstring RequestedStatistics::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("RequestedStatistics::ServiceConfigurationId");
    }
    Windows::Foundation::Collections::IVectorView<hstring> RequestedStatistics::Statistics()
    {
        PDNET_XSAPI_STUB("RequestedStatistics::Statistics");
    }
}
