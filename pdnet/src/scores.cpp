#include "pch.h"
#include "scores.h"
#include "eos.h"
#include "config.h"
#include "gamepatches.h"
#include "log.h"
#include <future>
#include <mutex>

#ifdef PDNET_NO_EOS

namespace pdnet::scores
{
    void SubmitRating(int) {}
    std::vector<Record> Ranks() { return {}; }
}

#else

#include "eossdk.h"

namespace pdnet::scores
{
    namespace
    {
        using namespace eossdk;

        std::string Narrow(const Api& api, EOS_ProductUserId id)
        {
            char buf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
            int32_t len = sizeof(buf);
            return id && api.EOS_ProductUserId_ToString(id, buf, &len) == EOS_EResult::EOS_Success ? std::string(buf) : std::string();
        }

        template <typename Info>
        struct Pending
        {
            std::function<void(const Info*)> fn;
            static void EOS_CALL Thunk(const Info* data)
            {
                auto self = static_cast<Pending*>(data->ClientData);
                self->fn(data);
                delete self;
            }
        };

        std::mutex g_lock;
        std::vector<Record> g_ranks;
        uint64_t g_ranksAt = 0;
        int g_lastSent = -1;
    }

    void SubmitRating(int rating)
    {
        if (!GetConfig().scoreboard || rating < 0 || !eos::SignedIn()) return;
        {
            std::lock_guard guard(g_lock);
            if (rating == g_lastSent) return;
            g_lastSent = rating;
        }
        std::string me = winrt::to_string(eos::LocalUserId()), stat = GetConfig().scoreboardStat;
        eos::Post([me, stat, rating](void* platform)
        {
            const Api* api = eossdk::Load(ModuleDir());
            if (!api || !platform) return;
            EOS_HStats stats = api->EOS_Platform_GetStatsInterface(static_cast<EOS_HPlatform>(platform));
            EOS_ProductUserId user = api->EOS_ProductUserId_FromString(me.c_str());
            EOS_Stats_IngestData data{ EOS_STATS_INGESTDATA_API_LATEST, stat.c_str(), rating };
            EOS_Stats_IngestStatOptions o{ EOS_STATS_INGESTSTAT_API_LATEST, user, &data, 1, user };
            auto done = new Pending<EOS_Stats_IngestStatCompleteCallbackInfo>{ [rating, stat](const EOS_Stats_IngestStatCompleteCallbackInfo* info)
            {
                const Api* api = eossdk::Load(ModuleDir());
                Log(L"scores: rating %d sent to stat %S - %s", rating, stat.c_str(), api ? Result(*api, info->ResultCode).c_str() : L"?");
                if (info->ResultCode != EOS_EResult::EOS_Success)
                {
                    std::lock_guard guard(g_lock);
                    g_lastSent = -1; // try again next time
                }
            } };
            api->EOS_Stats_IngestStat(stats, &o, done, &Pending<EOS_Stats_IngestStatCompleteCallbackInfo>::Thunk);
        });
    }

    std::vector<Record> Ranks()
    {
        if (!GetConfig().scoreboard || !eos::SignedIn()) return {};
        SubmitRating(gamepatches::SavedRating()); // once per session (and after any change): players show up before their next match
        {
            std::lock_guard guard(g_lock);
            if (g_ranksAt && GetTickCount64() - g_ranksAt < 20000) return g_ranks;
        }
        auto promise = std::make_shared<std::promise<std::vector<Record>>>();
        auto future = promise->get_future();
        std::string me = winrt::to_string(eos::LocalUserId()), board = GetConfig().scoreboardLeaderboard;
        eos::Post([me, board, promise](void* platform)
        {
            const Api* api = eossdk::Load(ModuleDir());
            if (!api || !platform)
            {
                promise->set_value({});
                return;
            }
            EOS_HLeaderboards boards = api->EOS_Platform_GetLeaderboardsInterface(static_cast<EOS_HPlatform>(platform));
            EOS_Leaderboards_QueryLeaderboardRanksOptions o{ EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST, board.c_str(),
                                                             api->EOS_ProductUserId_FromString(me.c_str()) };
            auto done = new Pending<EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo>{ [boards, me, board, promise](const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* info)
            {
                const Api* api = eossdk::Load(ModuleDir());
                std::vector<Record> out;
                if (info->ResultCode != EOS_EResult::EOS_Success)
                {
                    Log(L"scores: leaderboard %S query failed - %s", board.c_str(), Result(*api, info->ResultCode).c_str());
                    promise->set_value(std::move(out));
                    return;
                }
                EOS_Leaderboards_GetLeaderboardRecordCountOptions co{ EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST };
                uint32_t n = api->EOS_Leaderboards_GetLeaderboardRecordCount(boards, &co);
                for (uint32_t i = 0; i < n; ++i)
                {
                    EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions ro{ EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST, i };
                    EOS_Leaderboards_LeaderboardRecord* r = nullptr;
                    if (api->EOS_Leaderboards_CopyLeaderboardRecordByIndex(boards, &ro, &r) != EOS_EResult::EOS_Success || !r) continue;
                    Record rec;
                    rec.rank = r->Rank;
                    rec.score = r->Score;
                    rec.userId = Narrow(*api, r->UserId);
                    rec.name = r->UserDisplayName ? r->UserDisplayName : "";
                    rec.me = rec.userId == me;
                    if (rec.me && rec.name.empty()) rec.name = winrt::to_string(eos::DisplayName());
                    if (rec.name.empty()) rec.name = "Player " + rec.userId.substr(0, 6);
                    out.push_back(std::move(rec));
                    api->EOS_Leaderboards_LeaderboardRecord_Release(r);
                }
                Log(L"scores: leaderboard %S has %u players", board.c_str(), n);
                promise->set_value(std::move(out));
            } };
            api->EOS_Leaderboards_QueryLeaderboardRanks(boards, &o, done, &Pending<EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo>::Thunk);
        });
        if (future.wait_for(std::chrono::milliseconds(12000)) != std::future_status::ready)
        {
            Log(L"scores: leaderboard query timed out");
            return {};
        }
        auto ranks = future.get();
        std::lock_guard guard(g_lock);
        g_ranks = ranks;
        g_ranksAt = GetTickCount64();
        return ranks;
    }
}

#endif
