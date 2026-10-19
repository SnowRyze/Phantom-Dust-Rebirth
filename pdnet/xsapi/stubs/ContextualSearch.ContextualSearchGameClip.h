#pragma once
#include "ContextualSearch/ContextualSearchGameClip.g.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    struct ContextualSearchGameClip : ContextualSearchGameClipT<ContextualSearchGameClip>
    {
        ContextualSearchGameClip() = default;

        hstring ClipName();
        uint64_t DurationInSeconds();
        hstring GameClipId();
        hstring GameClipLocale();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipUriInfo> GameClipUris();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipThumbnail> Thumbnails();
        Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipType GameClipType();
        uint64_t Views();
        hstring XboxUserId();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipStat> Stats();
    };
}
