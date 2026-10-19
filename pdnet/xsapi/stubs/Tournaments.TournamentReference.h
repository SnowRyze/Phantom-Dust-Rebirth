#pragma once
#include "Tournaments/TournamentReference.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TournamentReference : TournamentReferenceT<TournamentReference>
    {
        TournamentReference() = default;

        hstring DefinitionName();
        hstring TournamentId();
        hstring Organizer();
        hstring ServiceConfigurationId();
    };
}
namespace winrt::Microsoft::Xbox::Services::Tournaments::factory_implementation
{
    struct TournamentReference : TournamentReferenceT<TournamentReference, implementation::TournamentReference>
    {
    };
}
