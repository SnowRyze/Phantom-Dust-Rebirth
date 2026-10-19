#pragma once
#include "ContextualSearch/ContextualSearchService.g.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    struct ContextualSearchService : ContextualSearchServiceT<ContextualSearchService>
    {
        ContextualSearchService() = default;

        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchConfiguredStat>> GetConfigurationAsync(uint32_t titleId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchBroadcast>> GetBroadcastsAsync(uint32_t titleId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchBroadcast>> GetBroadcastsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring searchQuery);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchBroadcast>> GetBroadcastsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring filterStatName, Microsoft::Xbox::Services::ContextualSearch::ContextualSearchFilterOperator filterOperator, hstring filterStatValue);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipsResult> GetGameClipsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring filterStatName, Microsoft::Xbox::Services::ContextualSearch::ContextualSearchFilterOperator filterOperator, hstring filterStatValue);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipsResult> GetGameClipsAsync(uint32_t titleId, uint32_t skipItems, uint32_t maxItems, hstring orderByStatName, bool orderAscending, hstring searchQuery);
    };
}
