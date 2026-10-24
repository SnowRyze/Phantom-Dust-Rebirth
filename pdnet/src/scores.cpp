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
    void AddWin(const std::string&) {}
    std::vector<Record> Ranks(const std::string&) { return {}; }
    std::map<std::string, std::string> DisplayNames(const std::vector<std::string>&) { return {}; }
    std::map<std::string, int32_t> UserScores(const std::string&, const std::vector<std::string>&) { return {}; }
}

#else

#include "eossdk.h"

namespace pdnet::scores
{
    namespace
    {
        using namespace eossdk;
        constexpr const char* kRating = "CurrentMPRanking";

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
        struct Cached { std::vector<Record> ranks; uint64_t at = 0; };
        std::map<std::string, Cached> g_ranks;
        int g_lastRating = -1;

        // Runs `work` on the EOS thread; it must call `finish` once. Waits up to 12 s here.
        template <typename T>
        T Run(const wchar_t* what, std::function<void(const Api&, EOS_HPlatform, EOS_ProductUserId, std::function<void(T)>)> work)
        {
            auto promise = std::make_shared<std::promise<T>>();
            auto done = std::make_shared<std::atomic<bool>>(false);
            std::function<void(T)> finish = [promise, done](T value) { if (!done->exchange(true)) promise->set_value(std::move(value)); };
            auto future = promise->get_future();
            std::string me = winrt::to_string(eos::LocalUserId());
            eos::Post([work = std::move(work), finish, me](void* platform)
            {
                const Api* api = eossdk::Load(ModuleDir());
                if (!api || !platform) { finish(T{}); return; }
                work(*api, static_cast<EOS_HPlatform>(platform), api->EOS_ProductUserId_FromString(me.c_str()), finish);
            });
            if (future.wait_for(std::chrono::milliseconds(12000)) != std::future_status::ready)
            {
                Log(L"scores: %s timed out", what);
                return T{};
            }
            return future.get();
        }

        // Sends stat amounts for this PC's player in one call (asynchronous). On failure `failed` runs (EOS thread).
        void Ingest(std::vector<std::pair<std::string, int>> amounts, std::function<void()> failed = nullptr)
        {
            std::string me = winrt::to_string(eos::LocalUserId());
            eos::Post([me, amounts, failed](void* platform)
            {
                const Api* api = eossdk::Load(ModuleDir());
                if (!api || !platform) return;
                EOS_HStats stats = api->EOS_Platform_GetStatsInterface(static_cast<EOS_HPlatform>(platform));
                EOS_ProductUserId user = api->EOS_ProductUserId_FromString(me.c_str());
                std::vector<EOS_Stats_IngestData> data;
                for (auto& [stat, amount] : amounts) data.push_back({ EOS_STATS_INGESTDATA_API_LATEST, stat.c_str(), amount });
                EOS_Stats_IngestStatOptions o{ EOS_STATS_INGESTSTAT_API_LATEST, user, data.data(), static_cast<uint32_t>(data.size()), user };
                std::wstring what = amounts.size() == 1 ? std::to_wstring(amounts[0].second) + L" sent to stat " + Wide(amounts[0].first.c_str())
                                                        : std::to_wstring(amounts.size()) + L" stats sent (" + Wide(amounts[0].first.c_str()) + L"...)";
                auto done = new Pending<EOS_Stats_IngestStatCompleteCallbackInfo>{ [what, failed](const EOS_Stats_IngestStatCompleteCallbackInfo* info)
                {
                    const Api* api = eossdk::Load(ModuleDir());
                    Log(L"scores: %s - %s", what.c_str(), api ? Result(*api, info->ResultCode).c_str() : L"?");
                    if (info->ResultCode != EOS_EResult::EOS_Success && failed) failed();
                } };
                api->EOS_Stats_IngestStat(stats, &o, done, &Pending<EOS_Stats_IngestStatCompleteCallbackInfo>::Thunk);
            });
        }

        // Player names on EOS: leaderboards and EOS Connect keep none for device-id accounts, so each player's name
        // (UTF-8, up to 32 bytes) is kept in kNameStats stats PlayerName0.. (aggregation Latest), 31 bits each
        // (always positive), low bits first.
        constexpr int kNameStats = 9;
        constexpr size_t kNameBytes = 32;
        bool g_nameSent = false;

        std::string NameStat(int i) { return "PlayerName" + std::to_string(i); }

        std::vector<int32_t> PackName(std::string name)
        {
            if (name.size() > kNameBytes)
            {
                size_t n = kNameBytes;
                while (n > 0 && (static_cast<unsigned char>(name[n]) & 0xC0) == 0x80) --n; // keep UTF-8 sequences whole
                name.resize(n);
            }
            std::vector<int32_t> out(kNameStats, 0);
            for (size_t bit = 0; bit < name.size() * 8; ++bit)
                if (static_cast<unsigned char>(name[bit / 8]) >> (bit % 8) & 1) out[bit / 31] |= 1 << (bit % 31);
            return out;
        }

        std::string UnpackName(const std::vector<int32_t>& values)
        {
            std::string name(kNameBytes, '\0');
            for (size_t bit = 0; bit < kNameBytes * 8; ++bit)
                if (values[bit / 31] >> (bit % 31) & 1) name[bit / 8] |= static_cast<char>(1 << (bit % 8));
            name.resize(strnlen(name.c_str(), kNameBytes));
            return name;
        }

        void SubmitName()
        {
            {
                std::lock_guard guard(g_lock);
                if (g_nameSent) return;
                g_nameSent = true;
            }
            std::vector<std::pair<std::string, int>> amounts;
            auto packed = PackName(winrt::to_string(eos::DisplayName()));
            for (int i = 0; i < kNameStats; ++i) amounts.emplace_back(NameStat(i), packed[i]);
            Ingest(amounts, [] { std::lock_guard guard(g_lock); g_nameSent = false; });
        }
    }

    void SubmitRating(int rating)
    {
        if (!GetConfig().scoreboard || rating < 0 || !eos::SignedIn()) return;
        {
            std::lock_guard guard(g_lock);
            if (rating == g_lastRating) return;
            g_lastRating = rating;
        }
        Ingest({ { kRating, rating } }, [] { std::lock_guard guard(g_lock); g_lastRating = -1; }); // failed: try again next time
    }

    void AddWin(const std::string& mode)
    {
        if (!GetConfig().scoreboard || !eos::SignedIn()) return;
        Ingest({ { mode, 1 } }); // Total Wins is the sum of the three modes (Leaderboard.LeaderboardResult.cpp)
        std::lock_guard guard(g_lock);
        g_ranks.erase(mode); // show it next time the board opens
    }

    std::vector<Record> Ranks(const std::string& board)
    {
        if (!GetConfig().scoreboard || !eos::SignedIn()) return {};
        SubmitRating(gamepatches::SavedRating()); // once per session (and after any change): players show up before their next match
        SubmitName();
        {
            std::lock_guard guard(g_lock);
            auto it = g_ranks.find(board);
            if (it != g_ranks.end() && GetTickCount64() - it->second.at < 20000) return it->second.ranks;
        }
        std::string me = winrt::to_string(eos::LocalUserId());
        auto ranks = Run<std::vector<Record>>(L"leaderboard query", [board, me](const Api& api, EOS_HPlatform platform, EOS_ProductUserId user, std::function<void(std::vector<Record>)> finish)
        {
            EOS_HLeaderboards boards = api.EOS_Platform_GetLeaderboardsInterface(platform);
            EOS_Leaderboards_QueryLeaderboardRanksOptions o{ EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST, board.c_str(), user };
            auto done = new Pending<EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo>{ [boards, board, me, finish](const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* info)
            {
                const Api* api = eossdk::Load(ModuleDir());
                std::vector<Record> out;
                if (info->ResultCode != EOS_EResult::EOS_Success)
                {
                    Log(L"scores: leaderboard %S query failed - %s", board.c_str(), Result(*api, info->ResultCode).c_str());
                    finish(std::move(out));
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
                    if (rec.me) Log(L"scores: %S: this player is rank %u with %d", board.c_str(), rec.rank, rec.score);
                    out.push_back(std::move(rec));
                    api->EOS_Leaderboards_LeaderboardRecord_Release(r);
                }
                Log(L"scores: leaderboard %S has %u players", board.c_str(), n);
                finish(std::move(out));
            } };
            api.EOS_Leaderboards_QueryLeaderboardRanks(boards, &o, done, &Pending<EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo>::Thunk);
        });
        // Leaderboard records carry no names for device-id accounts: each player's display_name is kept in the
        // PlayerName stats (sent every session), so a renamed player shows their new name on the same entry.
        std::vector<std::string> unnamed;
        for (auto& r : ranks) if (r.name.empty()) unnamed.push_back(r.userId);
        if (!unnamed.empty())
        {
            auto names = DisplayNames(unnamed);
            Log(L"scores: %zu of %zu player name(s) found", names.size(), unnamed.size());
            for (auto& r : ranks)
                if (r.name.empty())
                {
                    auto it = names.find(r.userId);
                    r.name = it != names.end() && !it->second.empty() ? it->second : "Player " + r.userId.substr(0, 6);
                }
        }
        std::lock_guard guard(g_lock);
        g_ranks[board] = { ranks, GetTickCount64() };
        return ranks;
    }

    std::map<std::string, std::string> DisplayNames(const std::vector<std::string>& userIds)
    {
        std::map<std::string, std::string> out;
        for (size_t at = 0; at < userIds.size(); at += 100)
        {
            std::vector<std::string> batch(userIds.begin() + at, userIds.begin() + (std::min)(userIds.size(), at + 100));
            out.merge(Run<std::map<std::string, std::string>>(L"player names query", [batch](const Api& api, EOS_HPlatform platform, EOS_ProductUserId user, std::function<void(std::map<std::string, std::string>)> finish)
            {
                EOS_HLeaderboards boards = api.EOS_Platform_GetLeaderboardsInterface(platform);
                std::vector<EOS_ProductUserId> ids;
                for (auto& id : batch) ids.push_back(api.EOS_ProductUserId_FromString(id.c_str()));
                auto statNames = std::make_shared<std::vector<std::string>>();
                for (int i = 0; i < kNameStats; ++i) statNames->push_back(NameStat(i));
                std::vector<EOS_Leaderboards_UserScoresQueryStatInfo> info;
                for (auto& n : *statNames) info.push_back({ EOS_LEADERBOARDS_USERSCORESQUERYSTATINFO_API_LATEST, n.c_str(), EOS_ELeaderboardAggregation::EOS_LA_Latest });
                EOS_Leaderboards_QueryLeaderboardUserScoresOptions o{ EOS_LEADERBOARDS_QUERYLEADERBOARDUSERSCORES_API_LATEST, ids.data(), static_cast<uint32_t>(ids.size()),
                                                                      info.data(), static_cast<uint32_t>(info.size()), EOS_LEADERBOARDS_TIME_UNDEFINED, EOS_LEADERBOARDS_TIME_UNDEFINED, user };
                auto done = new Pending<EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo>{ [boards, statNames, finish](const EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo* info)
                {
                    const Api* api = eossdk::Load(ModuleDir());
                    std::map<std::string, std::vector<int32_t>> values;
                    if (info->ResultCode != EOS_EResult::EOS_Success)
                        Log(L"scores: player names query failed - %s", Result(*api, info->ResultCode).c_str());
                    else
                        for (int s = 0; s < kNameStats; ++s)
                        {
                            const char* stat = (*statNames)[s].c_str();
                            EOS_Leaderboards_GetLeaderboardUserScoreCountOptions co{ EOS_LEADERBOARDS_GETLEADERBOARDUSERSCORECOUNT_API_LATEST, stat };
                            uint32_t n = api->EOS_Leaderboards_GetLeaderboardUserScoreCount(boards, &co);
                            for (uint32_t i = 0; i < n; ++i)
                            {
                                EOS_Leaderboards_CopyLeaderboardUserScoreByIndexOptions so{ EOS_LEADERBOARDS_COPYLEADERBOARDUSERSCOREBYINDEX_API_LATEST, i, stat };
                                EOS_Leaderboards_LeaderboardUserScore* score = nullptr;
                                if (api->EOS_Leaderboards_CopyLeaderboardUserScoreByIndex(boards, &so, &score) != EOS_EResult::EOS_Success || !score) continue;
                                auto& v = values[Narrow(*api, score->UserId)];
                                v.resize(kNameStats, 0);
                                v[s] = score->Score;
                                api->EOS_Leaderboards_LeaderboardUserScore_Release(score);
                            }
                        }
                    std::map<std::string, std::string> names;
                    for (auto& [id, v] : values) names[id] = UnpackName(v);
                    finish(std::move(names));
                } };
                api.EOS_Leaderboards_QueryLeaderboardUserScores(boards, &o, done, &Pending<EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo>::Thunk);
            }));
        }
        return out;
    }
    std::map<std::string, int32_t> UserScores(const std::string& stat, const std::vector<std::string>& userIds)
    {
        if (!GetConfig().scoreboard || !eos::SignedIn() || userIds.empty()) return {};
        return Run<std::map<std::string, int32_t>>(L"user scores query", [stat, userIds](const Api& api, EOS_HPlatform platform, EOS_ProductUserId user, std::function<void(std::map<std::string, int32_t>)> finish)
        {
            EOS_HLeaderboards boards = api.EOS_Platform_GetLeaderboardsInterface(platform);
            std::vector<EOS_ProductUserId> ids;
            for (auto& id : userIds) ids.push_back(api.EOS_ProductUserId_FromString(id.c_str()));
            EOS_Leaderboards_UserScoresQueryStatInfo info{ EOS_LEADERBOARDS_USERSCORESQUERYSTATINFO_API_LATEST, stat.c_str(), EOS_ELeaderboardAggregation::EOS_LA_Sum };
            EOS_Leaderboards_QueryLeaderboardUserScoresOptions o{ EOS_LEADERBOARDS_QUERYLEADERBOARDUSERSCORES_API_LATEST, ids.data(), static_cast<uint32_t>(ids.size()),
                                                                  &info, 1, EOS_LEADERBOARDS_TIME_UNDEFINED, EOS_LEADERBOARDS_TIME_UNDEFINED, user };
            auto done = new Pending<EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo>{ [boards, stat, finish](const EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo* info)
            {
                const Api* api = eossdk::Load(ModuleDir());
                std::map<std::string, int32_t> out;
                if (info->ResultCode != EOS_EResult::EOS_Success)
                {
                    Log(L"scores: %S scores query failed - %s", stat.c_str(), Result(*api, info->ResultCode).c_str());
                    finish(std::move(out));
                    return;
                }
                EOS_Leaderboards_GetLeaderboardUserScoreCountOptions co{ EOS_LEADERBOARDS_GETLEADERBOARDUSERSCORECOUNT_API_LATEST, stat.c_str() };
                uint32_t n = api->EOS_Leaderboards_GetLeaderboardUserScoreCount(boards, &co);
                for (uint32_t i = 0; i < n; ++i)
                {
                    EOS_Leaderboards_CopyLeaderboardUserScoreByIndexOptions so{ EOS_LEADERBOARDS_COPYLEADERBOARDUSERSCOREBYINDEX_API_LATEST, i, stat.c_str() };
                    EOS_Leaderboards_LeaderboardUserScore* s = nullptr;
                    if (api->EOS_Leaderboards_CopyLeaderboardUserScoreByIndex(boards, &so, &s) != EOS_EResult::EOS_Success || !s) continue;
                    out[Narrow(*api, s->UserId)] = s->Score;
                    api->EOS_Leaderboards_LeaderboardUserScore_Release(s);
                }
                finish(std::move(out));
            } };
            api.EOS_Leaderboards_QueryLeaderboardUserScores(boards, &o, done, &Pending<EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo>::Thunk);
        });
    }
}

#endif
