#pragma once
#include "Tournaments/TournamentRequest.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TournamentRequest : TournamentRequestT<TournamentRequest>
    {
        TournamentRequest() = default;

        TournamentRequest(bool filterResultsForUser);
        uint32_t MaxItems();
        void MaxItems(uint32_t value);
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TournamentState> StateFilter();
        void StateFilter(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TournamentState> const& states);
        Microsoft::Xbox::Services::Tournaments::TournamentOrderBy OrderBy();
        void OrderBy(Microsoft::Xbox::Services::Tournaments::TournamentOrderBy const& value);
        Microsoft::Xbox::Services::Tournaments::TournamentSortOrder SortOrder();
        void SortOrder(Microsoft::Xbox::Services::Tournaments::TournamentSortOrder const& value);
        hstring OrganizerId();
        void OrganizerId(hstring const& value);
    };
}
namespace winrt::Microsoft::Xbox::Services::Tournaments::factory_implementation
{
    struct TournamentRequest : TournamentRequestT<TournamentRequest, implementation::TournamentRequest>
    {
    };
}
