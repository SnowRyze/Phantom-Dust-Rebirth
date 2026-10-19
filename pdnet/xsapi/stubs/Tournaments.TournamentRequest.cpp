#include "xsapi_pch.h"
#include "Tournaments.TournamentRequest.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    TournamentRequest::TournamentRequest(bool filterResultsForUser)
    {
        PDNET_XSAPI_STUB("TournamentRequest::TournamentRequest");
    }
    uint32_t TournamentRequest::MaxItems()
    {
        PDNET_XSAPI_STUB("TournamentRequest::MaxItems");
    }
    void TournamentRequest::MaxItems(uint32_t value)
    {
        PDNET_XSAPI_STUB("TournamentRequest::MaxItems");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TournamentState> TournamentRequest::StateFilter()
    {
        PDNET_XSAPI_STUB("TournamentRequest::StateFilter");
    }
    void TournamentRequest::StateFilter(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TournamentState> const& states)
    {
        PDNET_XSAPI_STUB("TournamentRequest::StateFilter");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentOrderBy TournamentRequest::OrderBy()
    {
        PDNET_XSAPI_STUB("TournamentRequest::OrderBy");
    }
    void TournamentRequest::OrderBy(Microsoft::Xbox::Services::Tournaments::TournamentOrderBy const& value)
    {
        PDNET_XSAPI_STUB("TournamentRequest::OrderBy");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentSortOrder TournamentRequest::SortOrder()
    {
        PDNET_XSAPI_STUB("TournamentRequest::SortOrder");
    }
    void TournamentRequest::SortOrder(Microsoft::Xbox::Services::Tournaments::TournamentSortOrder const& value)
    {
        PDNET_XSAPI_STUB("TournamentRequest::SortOrder");
    }
    hstring TournamentRequest::OrganizerId()
    {
        PDNET_XSAPI_STUB("TournamentRequest::OrganizerId");
    }
    void TournamentRequest::OrganizerId(hstring const& value)
    {
        PDNET_XSAPI_STUB("TournamentRequest::OrganizerId");
    }
}
