#pragma once
#include "Tournaments/MatchMetadata.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct MatchMetadata : MatchMetadataT<MatchMetadata>
    {
        MatchMetadata() = default;

        hstring Description();
        Windows::Foundation::DateTime StartTIme();
        bool IsBye();
        Windows::Foundation::Collections::IVectorView<hstring> OpposingTeamIds();
    };
}
