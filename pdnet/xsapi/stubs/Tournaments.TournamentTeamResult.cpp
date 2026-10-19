#include "xsapi_pch.h"
#include "Tournaments.TournamentTeamResult.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Microsoft::Xbox::Services::Tournaments::TournamentGameResultState TournamentTeamResult::State()
    {
        PDNET_XSAPI_STUB("TournamentTeamResult::State");
    }
    void TournamentTeamResult::State(Microsoft::Xbox::Services::Tournaments::TournamentGameResultState const& value)
    {
        PDNET_XSAPI_STUB("TournamentTeamResult::State");
    }
    uint64_t TournamentTeamResult::Ranking()
    {
        PDNET_XSAPI_STUB("TournamentTeamResult::Ranking");
    }
    void TournamentTeamResult::Ranking(uint64_t value)
    {
        PDNET_XSAPI_STUB("TournamentTeamResult::Ranking");
    }
}
