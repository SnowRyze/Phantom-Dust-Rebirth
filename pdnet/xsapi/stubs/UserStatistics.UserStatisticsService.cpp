// pdnet-hand: the scoreboard's win counts (the Ratings board's Total Wins column comes from here, not from the
// leaderboard rows): read from EOS for the players the scoreboard listed (src/scores.h). Other statistics (such as
// the daily reward's RewardStat) are left out, as before.
#include "xsapi_pch.h"
#include "UserStatistics.UserStatisticsService.h"
#include "UserStatistics.UserStatisticsResult.h"
#include "UserStatistics.StatisticChangeSubscription.h"
#include "UserStatistics.ServiceConfigurationStatistic.h"
#include "UserStatistics.Statistic.h"
#include "../../src/scores.h"
#include <set>


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    namespace
    {
        using Results = std::vector<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>;

        // TotalWin is the three mode counts added up; the others are EOS stats of the same name.
        Results Lookup(Windows::Foundation::Collections::IVectorView<hstring> const& xuids, hstring const& scid, Windows::Foundation::Collections::IVectorView<hstring> const& names)
        {
            static const std::set<std::wstring> kKnown = { L"TotalWin", L"BattleRoyaleWin", L"TagTeamWin", L"OneOnOneWin", L"CurrentMPRanking" };
            std::vector<std::wstring> players;
            std::vector<std::string> ids;
            if (xuids)
                for (auto const& x : xuids)
                {
                    std::string id = ::pdnet::xsapi::EosUserOf(std::wstring{ x });
                    if (!id.empty()) { players.emplace_back(x); ids.push_back(id); }
                }
            std::map<std::wstring, std::map<std::string, int32_t>> values; // stat -> EOS user -> value
            if (names && !ids.empty())
                for (auto const& n : names)
                {
                    std::wstring name{ n };
                    if (!kKnown.count(name)) continue;
                    auto& out = values[name];
                    std::vector<std::string> stats{ winrt::to_string(name) };
                    if (name == L"TotalWin") stats = { "BattleRoyaleWin", "TagTeamWin", "OneOnOneWin" };
                    for (auto& s : stats)
                        for (auto& [id, v] : ::pdnet::scores::UserScores(s, ids)) out[id] += v;
                }
            Results results;
            for (size_t i = 0; i < players.size(); ++i)
            {
                std::vector<Microsoft::Xbox::Services::UserStatistics::Statistic> stats;
                for (auto& [name, byUser] : values)
                {
                    auto it = byUser.find(ids[i]);
                    stats.push_back(winrt::make<Statistic>(hstring{ name }, winrt::to_hstring(it == byUser.end() ? 0 : it->second)));
                }
                if (stats.empty()) continue;
                std::vector<Microsoft::Xbox::Services::UserStatistics::ServiceConfigurationStatistic> scids{ winrt::make<ServiceConfigurationStatistic>(scid, std::move(stats)) };
                results.push_back(winrt::make<UserStatisticsResult>(hstring{ players[i] }, std::move(scids)));
            }
            if (!values.empty()) ::pdnet::Log(L"xsapi-pdnet: statistics for %zu player(s), %zu stat(s)", results.size(), values.size());
            return results;
        }
    }

    winrt::event_token UserStatisticsService::StatisticChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::UserStatistics::StatisticChangeEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB_DEFAULT("UserStatisticsService::StatisticChanged"); return winrt::event_token{};
    }
    void UserStatisticsService::StatisticChanged(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_DEFAULT("UserStatisticsService::StatisticChanged");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult> UserStatisticsService::GetSingleUserStatisticsAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName)
    {
        co_await winrt::resume_background();
        auto r = Lookup(winrt::single_threaded_vector<hstring>({ xboxUserId }).GetView(), serviceConfigurationId, winrt::single_threaded_vector<hstring>({ statisticName }).GetView());
        co_return r.empty() ? winrt::make<UserStatisticsResult>() : r[0];
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult> UserStatisticsService::GetSingleUserStatisticsAsync(hstring xboxUserId, hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> statisticNames)
    {
        co_await winrt::resume_background();
        auto r = Lookup(winrt::single_threaded_vector<hstring>({ xboxUserId }).GetView(), serviceConfigurationId, statisticNames);
        co_return r.empty() ? winrt::make<UserStatisticsResult>() : r[0];
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>> UserStatisticsService::GetMultipleUserStatisticsAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> statisticNames)
    {
        co_await winrt::resume_background();
        co_return winrt::single_threaded_vector(Lookup(xboxUserIds, serviceConfigurationId, statisticNames)).GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>> UserStatisticsService::GetMultipleUserStatisticsForMultipleServiceConfigurationsAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::RequestedStatistics> requestedServiceConfigurationStatisticsCollection)
    {
        PDNET_XSAPI_STUB_DEFAULT("UserStatisticsService::GetMultipleUserStatisticsForMultipleServiceConfigurationsAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>().GetView();
    }
    Microsoft::Xbox::Services::UserStatistics::StatisticChangeSubscription UserStatisticsService::SubscribeToStatisticChange(hstring const& xboxUserId, hstring const& serviceConfigurationId, hstring const& statisticName)
    {
        PDNET_XSAPI_STUB_DEFAULT("UserStatisticsService::SubscribeToStatisticChange"); return winrt::make<winrt::Microsoft::Xbox::Services::UserStatistics::implementation::StatisticChangeSubscription>();
    }
    void UserStatisticsService::UnsubscribeFromStatisticChange(Microsoft::Xbox::Services::UserStatistics::StatisticChangeSubscription const& subscription)
    {
        PDNET_XSAPI_STUB_DEFAULT("UserStatisticsService::UnsubscribeFromStatisticChange");
    }
}
