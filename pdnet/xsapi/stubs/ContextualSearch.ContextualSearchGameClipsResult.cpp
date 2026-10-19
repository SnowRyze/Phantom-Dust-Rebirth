#include "xsapi_pch.h"
#include "ContextualSearch.ContextualSearchGameClipsResult.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipsResult> ContextualSearchGameClipsResult::GetNextAsync(uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("ContextualSearchGameClipsResult::GetNextAsync");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClip> ContextualSearchGameClipsResult::Items()
    {
        PDNET_XSAPI_STUB("ContextualSearchGameClipsResult::Items");
    }
}
