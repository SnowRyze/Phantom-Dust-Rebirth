#pragma once
#include "ContextualSearch/ContextualSearchGameClipsResult.g.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    struct ContextualSearchGameClipsResult : ContextualSearchGameClipsResultT<ContextualSearchGameClipsResult>
    {
        ContextualSearchGameClipsResult() = default;

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipsResult> GetNextAsync(uint32_t maxItems);
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClip> Items();
    };
}
