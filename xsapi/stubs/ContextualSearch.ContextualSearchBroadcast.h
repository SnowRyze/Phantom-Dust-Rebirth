#pragma once
#include "ContextualSearch/ContextualSearchBroadcast.g.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    struct ContextualSearchBroadcast : ContextualSearchBroadcastT<ContextualSearchBroadcast>
    {
        ContextualSearchBroadcast() = default;

        hstring XboxUserId();
        hstring Provider();
        hstring BroadcasterIdFromProvider();
        uint64_t Viewers();
        Windows::Foundation::DateTime StartedDate();
        Windows::Foundation::Collections::IMapView<hstring, hstring> CurrentStats();
    };
}
