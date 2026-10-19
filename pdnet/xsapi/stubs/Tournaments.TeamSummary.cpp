#include "xsapi_pch.h"
#include "Tournaments.TeamSummary.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    hstring TeamSummary::Id()
    {
        PDNET_XSAPI_STUB("TeamSummary::Id");
    }
    Microsoft::Xbox::Services::Tournaments::TeamState TeamSummary::TeamState()
    {
        PDNET_XSAPI_STUB("TeamSummary::TeamState");
    }
    uint64_t TeamSummary::Ranking()
    {
        PDNET_XSAPI_STUB("TeamSummary::Ranking");
    }
}
