#pragma once
#include "ContextualSearch/ContextualSearchGameClipThumbnail.g.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    struct ContextualSearchGameClipThumbnail : ContextualSearchGameClipThumbnailT<ContextualSearchGameClipThumbnail>
    {
        ContextualSearchGameClipThumbnail() = default;

        Windows::Foundation::Uri Url();
        uint64_t FileSize();
        Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipThumbnailType ThumbnailType();
    };
}
