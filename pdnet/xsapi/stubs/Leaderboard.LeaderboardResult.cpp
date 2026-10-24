// pdnet-hand: pages of the EOS leaderboards (src/scores.cpp).
#include "xsapi_pch.h"
#include "Leaderboard.LeaderboardResult.h"
#include "Leaderboard.LeaderboardRow.h"
#include "Leaderboard.LeaderboardColumn.h"
#include "Leaderboard.LeaderboardQuery.h"

namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    namespace
    {
        // This PC's player has their own id; others get a stable xuid-like number made from their EOS user id.
        std::wstring XuidOf(const ::pdnet::scores::Record& r)
        {
            if (r.me) return ::pdnet::xsapi::LocalXuid();
            uint64_t h = 1469598103934665603ull;
            for (unsigned char c : r.userId) h = (h ^ c) * 1099511628211ull;
            return std::to_wstring(2533274790395904ull + h % 1'000'000'000'000ull);
        }
    }

    LeaderboardResult::LeaderboardResult(std::wstring board, std::vector<Row> all, std::vector<std::wstring> columns, size_t start, uint32_t maxItems)
        : m_board(std::move(board)), m_all(std::move(all)), m_columns(std::move(columns)), m_start(start), m_max(maxItems ? maxItems : 10)
    {
        if (m_start > m_all.size()) m_start = m_all.size();
    }

    Microsoft::Xbox::Services::Leaderboard::LeaderboardResult LeaderboardResult::Page(const wchar_t* why, hstring const& board, hstring const& skipToXuid, uint32_t skipToRank, uint32_t maxItems,
                                                                                     Windows::Foundation::Collections::IVectorView<hstring> const& columns)
    {
        // The scoreboard asks for its boards by short names (table at 0x1403fb910); the EOS leaderboard ids are the
        // names of the stats they rank.
        hstring id = board;
        if (board == L"Rating") id = L"CurrentMPRanking";
        else if (board == L"BattleRoyal") id = L"BattleRoyaleWin";
        else if (board == L"TagTeam") id = L"TagTeamWin";
        else if (board == L"OneOnOne") id = L"OneOnOneWin";

        std::vector<std::wstring> extra;
        if (columns)
            for (auto const& c : columns) extra.emplace_back(c);
        if (id == L"CurrentMPRanking" && extra.empty()) extra.emplace_back(L"TotalWin");

        std::vector<Row> all;
        for (auto& r : ::pdnet::scores::Ranks(winrt::to_string(id)))
        {
            if (r.me) r.name = winrt::to_string(::pdnet::xsapi::NameOf(::pdnet::xsapi::LocalXuid())); // the current display_name
            ::pdnet::xsapi::RememberName(XuidOf(r), winrt::to_hstring(r.name).c_str()); // for ProfileService
            ::pdnet::xsapi::RememberUser(XuidOf(r), r.userId);                          // for UserStatisticsService
            all.push_back({ r, {} });
        }
        for (auto& stat : extra)
        {
            // Total Wins is the three mode counts added up (always matches the win boards).
            std::vector<std::string> stats{ winrt::to_string(stat) };
            if (stat == L"TotalWin") stats = { "BattleRoyaleWin", "TagTeamWin", "OneOnOneWin" };
            std::map<std::string, int32_t> scores;
            for (auto& s : stats)
                for (size_t i = 0; i < all.size(); i += 100)
                {
                    std::vector<std::string> ids;
                    for (size_t j = i; j < all.size() && j < i + 100; ++j) ids.push_back(all[j].record.userId);
                    for (auto& [id, value] : ::pdnet::scores::UserScores(s, ids)) scores[id] += value;
                }
            for (auto& row : all)
            {
                auto it = scores.find(row.record.userId);
                row.extra.push_back(it == scores.end() ? 0 : it->second);
            }
        }

        uint32_t max = maxItems ? maxItems : 10;
        size_t start = skipToRank > 1 ? skipToRank - 1 : 0;
        if (!skipToXuid.empty())
        {
            start = 0;
            for (size_t i = 0; i < all.size(); ++i)
                if (XuidOf(all[i].record) == std::wstring_view(skipToXuid)) { start = i - i % max; break; }
        }
        ::pdnet::Log(L"xsapi-pdnet: scoreboard %s '%s': %zu players, rows from rank %zu (%u per page), %zu extra column(s)",
            why, id.c_str(), all.size(), start + 1, max, extra.size());
        for (size_t i = 0; i < all.size() && i < 5; ++i)
            if (!all[i].extra.empty())
                ::pdnet::Log(L"xsapi-pdnet: scoreboard row %u '%S': %d, %s %d", all[i].record.rank, all[i].record.name.c_str(), all[i].record.score, extra[0].c_str(), all[i].extra[0]);
        return winrt::make<LeaderboardResult>(std::wstring(id), std::move(all), std::move(extra), start, max); // id: the stat the game matches the first column by
    }

    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn> LeaderboardResult::Columns()
    {
        auto columns = winrt::single_threaded_vector<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn>();
        columns.Append(winrt::make<LeaderboardColumn>(hstring{ m_board }));
        for (auto& c : m_columns) columns.Append(winrt::make<LeaderboardColumn>(hstring{ c }));
        return columns.GetView();
    }

    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow> LeaderboardResult::Rows()
    {
        auto rows = winrt::single_threaded_vector<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow>();
        for (size_t i = m_start; i < m_all.size() && i < m_start + m_max; ++i)
        {
            const auto& r = m_all[i].record;
            std::vector<hstring> values{ winrt::to_hstring(r.score) };
            for (int32_t v : m_all[i].extra) values.push_back(winrt::to_hstring(v));
            double percentile = static_cast<double>(r.rank) / m_all.size();
            rows.Append(winrt::make<LeaderboardRow>(winrt::to_hstring(r.name), hstring{ XuidOf(r) }, r.rank, percentile, std::move(values)));
        }
        return rows.GetView();
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardResult::GetNextAsync(uint32_t maxItems)
    {
        co_return winrt::make<LeaderboardResult>(m_board, m_all, m_columns, m_start + m_max, maxItems ? maxItems : static_cast<uint32_t>(m_max));
    }

    Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery LeaderboardResult::GetNextQuery()
    {
        PDNET_XSAPI_STUB_DEFAULT("LeaderboardResult::GetNextQuery"); return winrt::make<winrt::Microsoft::Xbox::Services::Leaderboard::implementation::LeaderboardQuery>();
    }
}
