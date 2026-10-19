#include "xsapi_pch.h"
#include "ContextualSearch.ContextualSearchConfiguredStat.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    hstring ContextualSearchConfiguredStat::Name()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::Name");
    }
    hstring ContextualSearchConfiguredStat::DataType()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::DataType");
    }
    Microsoft::Xbox::Services::ContextualSearch::ContextualSearchStatVisibility ContextualSearchConfiguredStat::Visibility()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::Visibility");
    }
    hstring ContextualSearchConfiguredStat::DisplayName()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::DisplayName");
    }
    bool ContextualSearchConfiguredStat::CanBeFiltered()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::CanBeFiltered");
    }
    bool ContextualSearchConfiguredStat::CanBeSorted()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::CanBeSorted");
    }
    Microsoft::Xbox::Services::ContextualSearch::ContextualSearchStatVisibility ContextualSearchConfiguredStat::DisplayType()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::DisplayType");
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> ContextualSearchConfiguredStat::ValueToDisplayName()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::ValueToDisplayName");
    }
    uint64_t ContextualSearchConfiguredStat::RangeMin()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::RangeMin");
    }
    uint64_t ContextualSearchConfiguredStat::RangeMax()
    {
        PDNET_XSAPI_STUB("ContextualSearchConfiguredStat::RangeMax");
    }
}
