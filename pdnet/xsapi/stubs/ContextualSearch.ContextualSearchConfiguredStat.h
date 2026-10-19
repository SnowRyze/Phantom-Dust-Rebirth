#pragma once
#include "ContextualSearch/ContextualSearchConfiguredStat.g.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    struct ContextualSearchConfiguredStat : ContextualSearchConfiguredStatT<ContextualSearchConfiguredStat>
    {
        ContextualSearchConfiguredStat() = default;

        hstring Name();
        hstring DataType();
        Microsoft::Xbox::Services::ContextualSearch::ContextualSearchStatVisibility Visibility();
        hstring DisplayName();
        bool CanBeFiltered();
        bool CanBeSorted();
        Microsoft::Xbox::Services::ContextualSearch::ContextualSearchStatVisibility DisplayType();
        Windows::Foundation::Collections::IMapView<hstring, hstring> ValueToDisplayName();
        uint64_t RangeMin();
        uint64_t RangeMax();
    };
}
