#pragma once
#include "Leaderboard/LeaderboardResult.g.h"
#include "../../src/scores.h"

// pdnet-hand: a page of an EOS leaderboard (see src/scores.h). Page(...) builds one: from a rank, or around a player.
namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    struct LeaderboardResult : LeaderboardResultT<LeaderboardResult>
    {
        struct Row
        {
            ::pdnet::scores::Record record;
            std::vector<int32_t> extra; // one per extra column
        };

        LeaderboardResult() = default;
        LeaderboardResult(std::wstring board, std::vector<Row> all, std::vector<std::wstring> columns, size_t start, uint32_t maxItems);

        // `board`: the game's leaderboard name (CurrentMPRanking, BattleRoyaleWin, TagTeamWin, OneOnOneWin), also the
        // EOS leaderboard id. `skipToXuid` non-empty: the page that holds that player (from the top if they're not on
        // the board); otherwise the page starting at `skipToRank` (1 = top; 0 counts as 1). `columns`: extra stats
        // shown after the board's own (the rating board always gets TotalWin).
        static Microsoft::Xbox::Services::Leaderboard::LeaderboardResult Page(const wchar_t* why, hstring const& board, hstring const& skipToXuid, uint32_t skipToRank, uint32_t maxItems,
                                                                              Windows::Foundation::Collections::IVectorView<hstring> const& columns = nullptr);

        hstring DisplayName() { return hstring{ m_board }; }
        uint32_t TotalRowCount() { return static_cast<uint32_t>(m_all.size()); }
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn> Columns();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow> Rows();
        bool HasNext() { return m_start + m_max < m_all.size(); }
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> GetNextAsync(uint32_t maxItems);
        Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery GetNextQuery();

    private:
        std::wstring m_board;
        std::vector<Row> m_all;
        std::vector<std::wstring> m_columns;
        size_t m_start = 0;
        size_t m_max = 0;
    };
}
