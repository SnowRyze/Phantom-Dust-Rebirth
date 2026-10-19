#pragma once
#include "ContextualSearch/ContextualSearchGameClipUriInfo.g.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    struct ContextualSearchGameClipUriInfo : ContextualSearchGameClipUriInfoT<ContextualSearchGameClipUriInfo>
    {
        ContextualSearchGameClipUriInfo() = default;

        Windows::Foundation::Uri Url();
        uint64_t FileSize();
        Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipUriType UriType();
        Windows::Foundation::DateTime Expiration();
    };
}
