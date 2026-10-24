// pdnet-hand: pages of the EOS leaderboard (src/scores.cpp).
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

    LeaderboardResult::LeaderboardResult(std::vector<::pdnet::scores::Record> all, size_t start, uint32_t maxItems)
        : m_all(std::move(all)), m_start(start), m_max(maxItems ? maxItems : 10)
    {
        if (m_start > m_all.size()) m_start = m_all.size();
    }

    Microsoft::Xbox::Services::Leaderboard::LeaderboardResult LeaderboardResult::Page(const wchar_t* why, hstring const& skipToXuid, uint32_t skipToRank, uint32_t maxItems)
    {
        auto all = ::pdnet::scores::Ranks();
        uint32_t max = maxItems ? maxItems : 10;
        size_t start = skipToRank > 1 ? skipToRank - 1 : 0;
        if (!skipToXuid.empty())
        {
            start = 0;
            for (size_t i = 0; i < all.size(); ++i)
                if (XuidOf(all[i]) == std::wstring_view(skipToXuid)) { start = i - i % max; break; }
        }
        ::pdnet::Log(L"xsapi-pdnet: scoreboard %s: %zu players, rows from rank %zu (%u per page)", why, all.size(), start + 1, max);
        return winrt::make<LeaderboardResult>(std::move(all), start, max);
    }

    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn> LeaderboardResult::Columns()
    {
        return winrt::single_threaded_vector<Microsoft::Xbox::Services::Leaderboard::LeaderboardColumn>({ winrt::make<LeaderboardColumn>() }).GetView();
    }

    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow> LeaderboardResult::Rows()
    {
        auto rows = winrt::single_threaded_vector<Microsoft::Xbox::Services::Leaderboard::LeaderboardRow>();
        for (size_t i = m_start; i < m_all.size() && i < m_start + m_max; ++i)
        {
            const auto& r = m_all[i];
            double percentile = m_all.empty() ? 0 : static_cast<double>(r.rank) / m_all.size();
            rows.Append(winrt::make<LeaderboardRow>(winrt::to_hstring(r.name), hstring{ XuidOf(r) }, r.rank, percentile, winrt::to_hstring(r.score)));
        }
        return rows.GetView();
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardResult::GetNextAsync(uint32_t maxItems)
    {
        co_return winrt::make<LeaderboardResult>(m_all, m_start + m_max, maxItems ? maxItems : static_cast<uint32_t>(m_max));
    }

    Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery LeaderboardResult::GetNextQuery()
    {
        PDNET_XSAPI_STUB_DEFAULT("LeaderboardResult::GetNextQuery"); return winrt::make<winrt::Microsoft::Xbox::Services::Leaderboard::implementation::LeaderboardQuery>();
    }
}
