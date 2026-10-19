#include "xsapi_pch.h"
#include "Tournaments.TeamInfo.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    hstring TeamInfo::Id()
    {
        PDNET_XSAPI_STUB("TeamInfo::Id");
    }
    hstring TeamInfo::Name()
    {
        PDNET_XSAPI_STUB("TeamInfo::Name");
    }
    Windows::Foundation::Collections::IVectorView<hstring> TeamInfo::MemberXboxUserIds()
    {
        PDNET_XSAPI_STUB("TeamInfo::MemberXboxUserIds");
    }
    Windows::Foundation::DateTime TeamInfo::RegistrationDate()
    {
        PDNET_XSAPI_STUB("TeamInfo::RegistrationDate");
    }
    hstring TeamInfo::Standing()
    {
        PDNET_XSAPI_STUB("TeamInfo::Standing");
    }
    Microsoft::Xbox::Services::Tournaments::TeamState TeamInfo::TeamState()
    {
        PDNET_XSAPI_STUB("TeamInfo::TeamState");
    }
    Microsoft::Xbox::Services::Tournaments::TeamCompletedReason TeamInfo::CompletedReason()
    {
        PDNET_XSAPI_STUB("TeamInfo::CompletedReason");
    }
    uint64_t TeamInfo::Ranking()
    {
        PDNET_XSAPI_STUB("TeamInfo::Ranking");
    }
    hstring TeamInfo::ContinuationUri()
    {
        PDNET_XSAPI_STUB("TeamInfo::ContinuationUri");
    }
    Microsoft::Xbox::Services::Tournaments::CurrentMatchMetadata TeamInfo::CurrentMatchMetadata()
    {
        PDNET_XSAPI_STUB("TeamInfo::CurrentMatchMetadata");
    }
    Microsoft::Xbox::Services::Tournaments::PreviousMatchMetadata TeamInfo::PreviousMatchMetadata()
    {
        PDNET_XSAPI_STUB("TeamInfo::PreviousMatchMetadata");
    }
}
