#pragma once
#include "Tournaments/TeamRequest.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TeamRequest : TeamRequestT<TeamRequest>
    {
        TeamRequest() = default;

        TeamRequest(hstring const& organizerId, hstring const& tournamentId, bool filterResultsForUser);
        uint32_t MaxItems();
        void MaxItems(uint32_t value);
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TeamState> StateFilter();
        void StateFilter(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TeamState> const& states);
        Microsoft::Xbox::Services::Tournaments::TeamOrderBy OrderBy();
        void OrderBy(Microsoft::Xbox::Services::Tournaments::TeamOrderBy const& value);
    };
}
namespace winrt::Microsoft::Xbox::Services::Tournaments::factory_implementation
{
    struct TeamRequest : TeamRequestT<TeamRequest, implementation::TeamRequest>
    {
    };
}
