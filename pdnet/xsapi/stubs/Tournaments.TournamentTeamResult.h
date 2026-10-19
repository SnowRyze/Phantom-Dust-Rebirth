#pragma once
#include "Tournaments/TournamentTeamResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TournamentTeamResult : TournamentTeamResultT<TournamentTeamResult>
    {
        TournamentTeamResult() = default;

        Microsoft::Xbox::Services::Tournaments::TournamentGameResultState State();
        void State(Microsoft::Xbox::Services::Tournaments::TournamentGameResultState const& value);
        uint64_t Ranking();
        void Ranking(uint64_t value);
    };
}
namespace winrt::Microsoft::Xbox::Services::Tournaments::factory_implementation
{
    struct TournamentTeamResult : TournamentTeamResultT<TournamentTeamResult, implementation::TournamentTeamResult>
    {
    };
}
