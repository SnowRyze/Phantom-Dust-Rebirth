#include "xsapi_pch.h"
#include "Tournaments.MatchMetadata.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    hstring MatchMetadata::Description()
    {
        PDNET_XSAPI_STUB("MatchMetadata::Description");
    }
    Windows::Foundation::DateTime MatchMetadata::StartTIme()
    {
        PDNET_XSAPI_STUB("MatchMetadata::StartTIme");
    }
    bool MatchMetadata::IsBye()
    {
        PDNET_XSAPI_STUB("MatchMetadata::IsBye");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MatchMetadata::OpposingTeamIds()
    {
        PDNET_XSAPI_STUB("MatchMetadata::OpposingTeamIds");
    }
}
