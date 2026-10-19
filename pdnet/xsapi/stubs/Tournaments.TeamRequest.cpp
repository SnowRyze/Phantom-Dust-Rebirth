#include "xsapi_pch.h"
#include "Tournaments.TeamRequest.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    TeamRequest::TeamRequest(hstring const& organizerId, hstring const& tournamentId, bool filterResultsForUser)
    {
        PDNET_XSAPI_STUB("TeamRequest::TeamRequest");
    }
    uint32_t TeamRequest::MaxItems()
    {
        PDNET_XSAPI_STUB("TeamRequest::MaxItems");
    }
    void TeamRequest::MaxItems(uint32_t value)
    {
        PDNET_XSAPI_STUB("TeamRequest::MaxItems");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TeamState> TeamRequest::StateFilter()
    {
        PDNET_XSAPI_STUB("TeamRequest::StateFilter");
    }
    void TeamRequest::StateFilter(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TeamState> const& states)
    {
        PDNET_XSAPI_STUB("TeamRequest::StateFilter");
    }
    Microsoft::Xbox::Services::Tournaments::TeamOrderBy TeamRequest::OrderBy()
    {
        PDNET_XSAPI_STUB("TeamRequest::OrderBy");
    }
    void TeamRequest::OrderBy(Microsoft::Xbox::Services::Tournaments::TeamOrderBy const& value)
    {
        PDNET_XSAPI_STUB("TeamRequest::OrderBy");
    }
}
