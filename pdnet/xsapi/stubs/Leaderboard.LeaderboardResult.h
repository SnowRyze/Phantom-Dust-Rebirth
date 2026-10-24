#pragma once
#include "Leaderboard/LeaderboardResult.g.h"
#include "../../src/scores.h"

// pdnet-hand: a page of the EOS leaderboard (see src/scores.h). Page(...) builds one: from a rank, or around a player.
namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardResult : LeaderboardResultT<LeaderboardResult>
    {
        LeaderboardResult() = default;
        LeaderboardResult(std::vector<::pdnet::scores::Record> all, size_t start, uint32_t maxItems);

        // `skipToXuid` non-empty: the page that holds that player (from the top if they're not on the board);
        // otherwise the page starting at `skipToRank` (1 = top; 0 counts as 1).
        static Microsoft::Xbox::Services::Leaderboard::LeaderboardResult Page(const wchar_t* why, hstring const& skipToXuid, uint32_t skipToRank, uint32_t maxItems);

        hstring DisplayName() { return L"Rating"; }
        uint32_t TotalRowCount() { return static_cast<uint32_t>(m_all.size()); }
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn> Columns();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow> Rows();
        bool HasNext() { return m_start + m_max < m_all.size(); }
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> GetNextAsync(uint32_t maxItems);
        Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery GetNextQuery();

    private:
        std::vector<::pdnet::scores::Record> m_all;
        size_t m_start = 0;
        size_t m_max = 0;
    };
}
