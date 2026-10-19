#include "xsapi_pch.h"
#include "ContextualSearch.ContextualSearchBroadcast.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    hstring ContextualSearchBroadcast::XboxUserId()
    {
        PDNET_XSAPI_STUB("ContextualSearchBroadcast::XboxUserId");
    }
    hstring ContextualSearchBroadcast::Provider()
    {
        PDNET_XSAPI_STUB("ContextualSearchBroadcast::Provider");
    }
    hstring ContextualSearchBroadcast::BroadcasterIdFromProvider()
    {
        PDNET_XSAPI_STUB("ContextualSearchBroadcast::BroadcasterIdFromProvider");
    }
    uint64_t ContextualSearchBroadcast::Viewers()
    {
        PDNET_XSAPI_STUB("ContextualSearchBroadcast::Viewers");
    }
    Windows::Foundation::DateTime ContextualSearchBroadcast::StartedDate()
    {
        PDNET_XSAPI_STUB("ContextualSearchBroadcast::StartedDate");
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> ContextualSearchBroadcast::CurrentStats()
    {
        PDNET_XSAPI_STUB("ContextualSearchBroadcast::CurrentStats");
    }
}
