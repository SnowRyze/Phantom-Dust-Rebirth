#include "xsapi_pch.h"
#include "ContextualSearch.ContextualSearchService.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchConfiguredStat>> ContextualSearchService::GetConfigurationAsync(uint32_t titleId)
    {
        PDNET_XSAPI_STUB("ContextualSearchService::GetConfigurationAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchBroadcast>> ContextualSearchService::GetBroadcastsAsync(uint32_t titleId)
    {
        PDNET_XSAPI_STUB("ContextualSearchService::GetBroadcastsAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchBroadcast>> ContextualSearchService::GetBroadcastsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring searchQuery)
    {
        PDNET_XSAPI_STUB("ContextualSearchService::GetBroadcastsAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchBroadcast>> ContextualSearchService::GetBroadcastsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring filterStatName, Microsoft::Xbox::Services::ContextualSearch::ContextualSearchFilterOperator filterOperator, hstring filterStatValue)
    {
        PDNET_XSAPI_STUB("ContextualSearchService::GetBroadcastsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipsResult> ContextualSearchService::GetGameClipsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring filterStatName, Microsoft::Xbox::Services::ContextualSearch::ContextualSearchFilterOperator filterOperator, hstring filterStatValue)
    {
        PDNET_XSAPI_STUB("ContextualSearchService::GetGameClipsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipsResult> ContextualSearchService::GetGameClipsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring searchQuery)
    {
        PDNET_XSAPI_STUB("ContextualSearchService::GetGameClipsAsync");
    }
}
