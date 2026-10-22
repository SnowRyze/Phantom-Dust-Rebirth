#include "pch.h"
#include "lobbies.h"
#include "eos.h"
#include "config.h"
#include "gamepatches.h"
#include "log.h"
#include <condition_variable>
#include <deque>
#include <set>
#include <future>
#include <thread>

namespace pdnet::lobbies
{
    // Pieces of a long value (bytes). EOS accepts longer string values, but small pieces keep well inside its limits.
    constexpr size_t kPiece = 500;

    void Split(Attributes& out, const std::string& key, const std::string& value)
    {
        if (value.size() <= kPiece)
        {
            out[key] = value;
            return;
        }
        size_t pos = 0;
        int n = 0;
        while (pos < value.size())
        {
            size_t len = (std::min)(kPiece, value.size() - pos);
            while (pos + len < value.size() && len > 1 && (static_cast<unsigned char>(value[pos + len]) & 0xC0) == 0x80) --len; // keep UTF-8 sequences whole
            out[n ? key + "." + std::to_string(n) : key] = value.substr(pos, len);
            pos += len;
            ++n;
        }
        out[key + "#"] = std::to_string(n);
    }

    std::string Joined(const Attributes& in, const std::string& key)
    {
        auto first = in.find(key);
        if (first == in.end()) return {};
        std::string value = first->second;
        auto count = in.find(key + "#");
        int n = count == in.end() ? 1 : atoi(count->second.c_str());
        for (int i = 1; i < n; ++i)
        {
            auto piece = in.find(key + "." + std::to_string(i));
            if (piece != in.end()) value += piece->second;
        }
        return value;
    }

    std::vector<std::string> SplitKeys(const Attributes& in, const std::string& key)
    {
        std::vector<std::string> keys;
        if (in.count(key)) keys.push_back(key);
        auto count = in.find(key + "#");
        if (count != in.end())
        {
            keys.push_back(key + "#");
            int n = atoi(count->second.c_str());
            for (int i = 1; i < n; ++i)
                if (in.count(key + "." + std::to_string(i))) keys.push_back(key + "." + std::to_string(i));
        }
        return keys;
    }
}

#ifdef PDNET_NO_EOS

namespace pdnet::lobbies
{
    bool Available() { return false; }
    std::string LocalUserId() { return {}; }
    bool IsTalking(const char*) { return false; }
    bool IsMicOn(const char*) { return false; }
    std::optional<Lobby> Create(const std::string&, const std::string&, uint32_t, const Attributes&, const Attributes&) { return std::nullopt; }
    std::optional<Lobby> Join(const std::string&, const Attributes&) { return std::nullopt; }
    bool Update(const std::string&, const Changes&, const Changes&, uint32_t) { return false; }
    bool Leave(const std::string&) { return false; }
    bool Destroy(const std::string&) { return false; }
    bool Promote(const std::string&, const std::string&) { return false; }
    std::optional<Lobby> Read(const std::string&) { return std::nullopt; }
    std::vector<Lobby> Search(const std::string&, uint32_t) { return {}; }
    void SetListener(std::function<void(const std::string&, Event, const std::string&)>) {}
    void SetSignOutListener(std::function<void()>) {}
}

#else

#include "eossdk.h"

namespace pdnet::lobbies
{
    namespace
    {
        using namespace eossdk;
        constexpr uint32_t kTimeoutMs = 12000;

        const Api* Sdk() { return eossdk::Load(ModuleDir()); }

        std::string Narrow(const Api& api, EOS_ProductUserId id)
        {
            char buf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
            int32_t len = sizeof(buf);
            return id && api.EOS_ProductUserId_ToString(id, buf, &len) == EOS_EResult::EOS_Success ? std::string(buf) : std::string();
        }

        std::wstring W(const std::string& s) { return Wide(s.c_str()); }

        // One EOS completion callback, kept until EOS reports a final result.
        template <typename Info>
        struct Pending
        {
            std::function<void(const Info*)> fn;
            static void EOS_CALL Thunk(const Info* data)
            {
                if (data->ResultCode == EOS_EResult::EOS_OperationWillRetry) return; // EOS calls again later
                auto self = static_cast<Pending*>(data->ClientData);
                self->fn(data);
                delete self;
            }
        };

        template <typename Info>
        Pending<Info>* Then(std::function<void(const Info*)> fn) { return new Pending<Info>{ std::move(fn) }; }

        struct Context
        {
            const Api& api;
            EOS_HPlatform platform;
            EOS_HLobby lobby;
            EOS_ProductUserId me;
        };

        void RegisterNotifications(const Api& api, EOS_HPlatform platform, EOS_HLobby lobby);

        // Runs `work` on the EOS thread and waits (here, on the caller's thread) until it calls `finish` or time runs out.
        // On timeout the caller gets T{}; a late finish is dropped.
        template <typename T>
        T Run(const wchar_t* what, std::function<void(const Context&, std::function<void(T)>)> work, uint32_t timeoutMs = kTimeoutMs)
        {
            if (!eos::SignedIn())
            {
                Log(L"lobby: %s skipped - EOS is not signed in", what);
                return T{};
            }
            auto promise = std::make_shared<std::promise<T>>();
            auto done = std::make_shared<std::atomic<bool>>(false);
            std::function<void(T)> finish = [promise, done](T value)
            {
                if (!done->exchange(true)) promise->set_value(std::move(value));
            };
            auto future = promise->get_future();
            std::string userId = winrt::to_string(eos::LocalUserId());
            eos::Post([work = std::move(work), finish, userId, what](void* platform)
            {
                const Api* api = Sdk();
                if (!api || !platform)
                {
                    finish(T{});
                    return;
                }
                Context c{ *api, static_cast<EOS_HPlatform>(platform), api->EOS_Platform_GetLobbyInterface(static_cast<EOS_HPlatform>(platform)),
                           api->EOS_ProductUserId_FromString(userId.c_str()) };
                RegisterNotifications(*api, c.platform, c.lobby);
                try { work(c, finish); }
                catch (...)
                {
                    Log(L"lobby: %s threw 0x%08X", what, winrt::to_hresult());
                    finish(T{});
                }
            });
            if (future.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready)
            {
                Log(L"lobby: %s timed out after %u ms", what, timeoutMs);
                return T{};
            }
            return future.get();
        }

        std::string AttributeValue(const EOS_Lobby_AttributeData& d)
        {
            switch (d.ValueType)
            {
            case EOS_ELobbyAttributeType::EOS_AT_STRING: return d.Value.AsUtf8 ? d.Value.AsUtf8 : "";
            case EOS_ELobbyAttributeType::EOS_AT_INT64: return std::to_string(d.Value.AsInt64);
            case EOS_ELobbyAttributeType::EOS_AT_DOUBLE: return std::to_string(d.Value.AsDouble);
            case EOS_ELobbyAttributeType::EOS_AT_BOOLEAN: return d.Value.AsBool ? "1" : "0";
            }
            return {};
        }

        // Copies everything out of a details handle (its strings die with it).
        Lobby FromDetails(const Api& api, EOS_HLobbyDetails details, bool joined)
        {
            Lobby lobby;
            lobby.joined = joined;
            EOS_LobbyDetails_CopyInfoOptions infoOpts{ EOS_LOBBYDETAILS_COPYINFO_API_LATEST };
            EOS_LobbyDetails_Info* info = nullptr;
            if (api.EOS_LobbyDetails_CopyInfo(details, &infoOpts, &info) == EOS_EResult::EOS_Success && info)
            {
                lobby.id = info->LobbyId ? info->LobbyId : "";
                lobby.ownerId = Narrow(api, info->LobbyOwnerUserId);
                lobby.bucket = info->BucketId ? info->BucketId : "";
                lobby.maxMembers = info->MaxMembers;
                api.EOS_LobbyDetails_Info_Release(info);
            }

            EOS_LobbyDetails_GetAttributeCountOptions countOpts{ EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST };
            uint32_t count = api.EOS_LobbyDetails_GetAttributeCount(details, &countOpts);
            for (uint32_t i = 0; i < count; ++i)
            {
                EOS_LobbyDetails_CopyAttributeByIndexOptions o{ EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST, i };
                EOS_Lobby_Attribute* a = nullptr;
                if (api.EOS_LobbyDetails_CopyAttributeByIndex(details, &o, &a) == EOS_EResult::EOS_Success && a)
                {
                    if (a->Data && a->Data->Key) lobby.attributes[a->Data->Key] = AttributeValue(*a->Data);
                    api.EOS_Lobby_Attribute_Release(a);
                }
            }

            EOS_LobbyDetails_GetMemberCountOptions membersOpts{ EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST };
            uint32_t members = api.EOS_LobbyDetails_GetMemberCount(details, &membersOpts);
            lobby.memberCount = members;
            for (uint32_t i = 0; i < members; ++i)
            {
                EOS_LobbyDetails_GetMemberByIndexOptions o{ EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST, i };
                EOS_ProductUserId user = api.EOS_LobbyDetails_GetMemberByIndex(details, &o);
                if (!user) continue; // search result: members are not visible to non-members
                Member m;
                m.userId = Narrow(api, user);
                EOS_LobbyDetails_GetMemberAttributeCountOptions co{ EOS_LOBBYDETAILS_GETMEMBERATTRIBUTECOUNT_API_LATEST, user };
                uint32_t n = api.EOS_LobbyDetails_GetMemberAttributeCount(details, &co);
                for (uint32_t k = 0; k < n; ++k)
                {
                    EOS_LobbyDetails_CopyMemberAttributeByIndexOptions ao{ EOS_LOBBYDETAILS_COPYMEMBERATTRIBUTEBYINDEX_API_LATEST, user, k };
                    EOS_Lobby_Attribute* a = nullptr;
                    if (api.EOS_LobbyDetails_CopyMemberAttributeByIndex(details, &ao, &a) == EOS_EResult::EOS_Success && a)
                    {
                        if (a->Data && a->Data->Key) m.attributes[a->Data->Key] = AttributeValue(*a->Data);
                        api.EOS_Lobby_Attribute_Release(a);
                    }
                }
                lobby.members.push_back(std::move(m));
            }
            return lobby;
        }

        // The live copy of a lobby this PC is in, if it is in it.
        std::optional<Lobby> Local(const Context& c, const std::string& id)
        {
            EOS_Lobby_CopyLobbyDetailsHandleOptions o{ EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST, id.c_str(), c.me };
            EOS_HLobbyDetails details = nullptr;
            if (c.api.EOS_Lobby_CopyLobbyDetailsHandle(c.lobby, &o, &details) != EOS_EResult::EOS_Success || !details) return std::nullopt;
            Lobby lobby = FromDetails(c.api, details, true);
            c.api.EOS_LobbyDetails_Release(details);
            return lobby;
        }

        // Sends one modification (lobby changes only if `lobby` is set, i.e. we own it) and reports EOS's answer.
        void Modify(const Context& c, const std::string& id, const Changes* lobby, const Changes& member, uint32_t maxMembers, std::function<void(bool)> done)
        {
            EOS_Lobby_UpdateLobbyModificationOptions mo{ EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST, c.me, id.c_str() };
            EOS_HLobbyModification mod = nullptr;
            EOS_EResult r = c.api.EOS_Lobby_UpdateLobbyModification(c.lobby, &mo, &mod);
            if (r != EOS_EResult::EOS_Success || !mod)
            {
                Log(L"lobby: modify %S: %s", id.c_str(), Result(c.api, r).c_str());
                done(false);
                return;
            }

            bool any = false;
            auto check = [&](EOS_EResult result, const char* op, const std::string& key)
            {
                if (result != EOS_EResult::EOS_Success) Log(L"lobby: %S %S on %S: %s", op, key.c_str(), id.c_str(), Result(c.api, result).c_str());
                else any = true;
            };
            auto attribute = [](const std::string& key, const std::string& value)
            {
                EOS_Lobby_AttributeData a{};
                a.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
                a.Key = key.c_str();
                a.Value.AsUtf8 = value.c_str();
                a.ValueType = EOS_ELobbyAttributeType::EOS_AT_STRING;
                return a;
            };
            if (lobby)
            {
                for (auto& [key, value] : lobby->set)
                {
                    auto a = attribute(key, value);
                    EOS_LobbyModification_AddAttributeOptions o{ EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST, &a, EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC };
                    check(c.api.EOS_LobbyModification_AddAttribute(mod, &o), "set", key);
                }
                for (auto& key : lobby->remove)
                {
                    EOS_LobbyModification_RemoveAttributeOptions o{ EOS_LOBBYMODIFICATION_REMOVEATTRIBUTE_API_LATEST, key.c_str() };
                    check(c.api.EOS_LobbyModification_RemoveAttribute(mod, &o), "remove", key);
                }
                if (maxMembers)
                {
                    EOS_LobbyModification_SetMaxMembersOptions o{ EOS_LOBBYMODIFICATION_SETMAXMEMBERS_API_LATEST, maxMembers };
                    check(c.api.EOS_LobbyModification_SetMaxMembers(mod, &o), "max members", std::to_string(maxMembers));
                }
            }
            for (auto& [key, value] : member.set)
            {
                auto a = attribute(key, value);
                EOS_LobbyModification_AddMemberAttributeOptions o{ EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST, &a, EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC };
                check(c.api.EOS_LobbyModification_AddMemberAttribute(mod, &o), "set member", key);
            }
            for (auto& key : member.remove)
            {
                EOS_LobbyModification_RemoveMemberAttributeOptions o{ EOS_LOBBYMODIFICATION_REMOVEMEMBERATTRIBUTE_API_LATEST, key.c_str() };
                check(c.api.EOS_LobbyModification_RemoveMemberAttribute(mod, &o), "remove member", key);
            }

            if (!any)
            {
                c.api.EOS_LobbyModification_Release(mod);
                done(true);
                return;
            }
            EOS_Lobby_UpdateLobbyOptions uo{ EOS_LOBBY_UPDATELOBBY_API_LATEST, mod };
            const Api& api = c.api;
            c.api.EOS_Lobby_UpdateLobby(c.lobby, &uo, Then<EOS_Lobby_UpdateLobbyCallbackInfo>([&api, id, done](const EOS_Lobby_UpdateLobbyCallbackInfo* info)
            {
                if (info->ResultCode != EOS_EResult::EOS_Success) Log(L"lobby: update %S failed: %s", id.c_str(), Result(api, info->ResultCode).c_str());
                done(info->ResultCode == EOS_EResult::EOS_Success);
            }), Pending<EOS_Lobby_UpdateLobbyCallbackInfo>::Thunk);
            c.api.EOS_LobbyModification_Release(mod);
        }

        // Search (by bucket or by id); copies each result.
        void Find(const Context& c, const std::string* bucket, const std::string* id, uint32_t maxResults, std::function<void(std::vector<Lobby>)> done)
        {
            EOS_Lobby_CreateLobbySearchOptions so{ EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST, (std::min)(maxResults, static_cast<uint32_t>(EOS_LOBBY_MAX_SEARCH_RESULTS)) };
            EOS_HLobbySearch search = nullptr;
            EOS_EResult r = c.api.EOS_Lobby_CreateLobbySearch(c.lobby, &so, &search);
            if (r != EOS_EResult::EOS_Success || !search)
            {
                Log(L"lobby: create search: %s", Result(c.api, r).c_str());
                done({});
                return;
            }
            if (id)
            {
                EOS_LobbySearch_SetLobbyIdOptions o{ EOS_LOBBYSEARCH_SETLOBBYID_API_LATEST, id->c_str() };
                r = c.api.EOS_LobbySearch_SetLobbyId(search, &o);
            }
            else
            {
                EOS_Lobby_AttributeData a{};
                a.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
                a.Key = EOS_LOBBY_SEARCH_BUCKET_ID;
                a.Value.AsUtf8 = bucket->c_str();
                a.ValueType = EOS_ELobbyAttributeType::EOS_AT_STRING;
                EOS_LobbySearch_SetParameterOptions o{ EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST, &a, EOS_EComparisonOp::EOS_CO_EQUAL };
                r = c.api.EOS_LobbySearch_SetParameter(search, &o);
            }
            if (r != EOS_EResult::EOS_Success)
            {
                Log(L"lobby: search parameter: %s", Result(c.api, r).c_str());
                c.api.EOS_LobbySearch_Release(search);
                done({});
                return;
            }
            EOS_LobbySearch_FindOptions fo{ EOS_LOBBYSEARCH_FIND_API_LATEST, c.me };
            const Api& api = c.api;
            c.api.EOS_LobbySearch_Find(search, &fo, Then<EOS_LobbySearch_FindCallbackInfo>([&api, search, done](const EOS_LobbySearch_FindCallbackInfo* info)
            {
                std::vector<Lobby> found;
                if (info->ResultCode == EOS_EResult::EOS_Success)
                {
                    EOS_LobbySearch_GetSearchResultCountOptions co{ EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST };
                    uint32_t n = api.EOS_LobbySearch_GetSearchResultCount(search, &co);
                    for (uint32_t i = 0; i < n; ++i)
                    {
                        EOS_LobbySearch_CopySearchResultByIndexOptions o{ EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST, i };
                        EOS_HLobbyDetails details = nullptr;
                        if (api.EOS_LobbySearch_CopySearchResultByIndex(search, &o, &details) == EOS_EResult::EOS_Success && details)
                        {
                            found.push_back(FromDetails(api, details, false));
                            api.EOS_LobbyDetails_Release(details);
                        }
                    }
                }
                else if (info->ResultCode != EOS_EResult::EOS_NotFound)
                {
                    Log(L"lobby: search failed: %s", Result(api, info->ResultCode).c_str());
                }
                api.EOS_LobbySearch_Release(search);
                done(std::move(found));
            }), Pending<EOS_LobbySearch_FindCallbackInfo>::Thunk);
        }

        // Notifications are handed to one worker thread in arrival order, so the listener never runs on the EOS thread.
        std::mutex g_lock;
        std::condition_variable g_wake;
        std::deque<std::function<void()>> g_queue;
        std::function<void(const std::string&, Event, const std::string&)> g_listener;
        std::function<void()> g_signOutListener;

        void Dispatch(std::function<void()> item)
        {
            static std::once_flag once;
            std::call_once(once, []
            {
                std::thread([]
                {
                    winrt::init_apartment(winrt::apartment_type::multi_threaded);
                    for (;;)
                    {
                        std::function<void()> next;
                        {
                            std::unique_lock guard(g_lock);
                            g_wake.wait(guard, [] { return !g_queue.empty(); });
                            next = std::move(g_queue.front());
                            g_queue.pop_front();
                        }
                        try { next(); }
                        catch (...) { Log(L"lobby: listener threw 0x%08X", winrt::to_hresult()); }
                    }
                }).detach();
            });
            {
                std::lock_guard guard(g_lock);
                g_queue.push_back(std::move(item));
            }
            g_wake.notify_one();
        }

        void Notify(const char* lobbyId, Event event, std::string userId)
        {
            std::string id = lobbyId ? lobbyId : "";
            Dispatch([id, event, userId]
            {
                std::function<void(const std::string&, Event, const std::string&)> listener;
                {
                    std::lock_guard guard(g_lock);
                    listener = g_listener;
                }
                if (listener) listener(id, event, userId);
            });
        }

        void EOS_CALL OnLobbyUpdate(const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* info)
        {
            Notify(info->LobbyId, Event::LobbyUpdated, {});
        }

        void EOS_CALL OnMemberUpdate(const EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo* info)
        {
            Notify(info->LobbyId, Event::MemberUpdated, Narrow(*Sdk(), info->TargetUserId));
        }

        void EOS_CALL OnMemberStatus(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* info)
        {
            Event e = Event::MemberUpdated;
            switch (info->CurrentStatus)
            {
            case EOS_ELobbyMemberStatus::EOS_LMS_JOINED: e = Event::MemberJoined; break;
            case EOS_ELobbyMemberStatus::EOS_LMS_LEFT:
            case EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED: e = Event::MemberLeft; break;
            case EOS_ELobbyMemberStatus::EOS_LMS_KICKED: e = Event::Kicked; break;
            case EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED: e = Event::Promoted; break;
            case EOS_ELobbyMemberStatus::EOS_LMS_CLOSED: e = Event::Closed; break;
            }
            Notify(info->LobbyId, e, Narrow(*Sdk(), info->TargetUserId));
        }

        // Voice (voice_chat=on). Every lobby gets an EOS voice room, joined automatically with the lobby. Two
        // switches: the mic follows the game's own Voice Chat setting (shown as "Mic: On/Off" in the lobby, see
        // gamepatches::GameMicOn), hearing the others follows voice_listen. The game's original option muted both
        // ways; GameChat itself stays dead (it needs Xbox Live), so there is no second voice path.
        struct VoiceRoom
        {
            std::string name;
            int mic = -1, listen = -1;
            EOS_NotificationId participants = EOS_INVALID_NOTIFICATIONID, speaking = EOS_INVALID_NOTIFICATIONID;
            EOS_NotificationId sending = EOS_INVALID_NOTIFICATIONID, input = EOS_INVALID_NOTIFICATIONID;
        };
        std::map<std::string, VoiceRoom> g_voiceRooms; // EOS thread only: connected rooms by lobby id
        EOS_HLobby g_lobbyHandle = nullptr;            // EOS thread only
        EOS_HPlatform g_voicePlatform = nullptr;
        std::atomic<bool> g_wantMic{ true }, g_wantListen{ true };

        void EOS_CALL OnSending(const EOS_RTCAudio_UpdateSendingCallbackInfo* info)
        {
            if (info->ResultCode != EOS_EResult::EOS_Success) Log(L"voice: mic switch failed: %s", Result(*Sdk(), info->ResultCode).c_str());
        }

        void EOS_CALL OnReceiving(const EOS_RTCAudio_UpdateReceivingCallbackInfo* info)
        {
            if (info->ResultCode != EOS_EResult::EOS_Success) Log(L"voice: listen switch failed: %s", Result(*Sdk(), info->ResultCode).c_str());
        }

        // Sends the wanted mic / listen state to one connected room if it differs from what it has.
        void ApplyVoice(const Api& api, EOS_ProductUserId me, VoiceRoom& room)
        {
            EOS_HRTCAudio audio = api.EOS_RTC_GetAudioInterface(api.EOS_Platform_GetRTCInterface(g_voicePlatform));
            if (!audio) return;
            int mic = g_wantMic ? 1 : 0, listen = g_wantListen ? 1 : 0;
            if (room.mic != mic)
            {
                EOS_RTCAudio_UpdateSendingOptions o{ EOS_RTCAUDIO_UPDATESENDING_API_LATEST, me, room.name.c_str(), mic ? EOS_ERTCAudioStatus::EOS_RTCAS_Enabled : EOS_ERTCAudioStatus::EOS_RTCAS_Disabled };
                api.EOS_RTCAudio_UpdateSending(audio, &o, nullptr, OnSending);
                room.mic = mic;
                Log(L"voice: mic %s", mic ? L"on" : L"off");
            }
            if (room.listen != listen)
            {
                EOS_RTCAudio_UpdateReceivingOptions o{ EOS_RTCAUDIO_UPDATERECEIVING_API_LATEST, me, room.name.c_str(), nullptr, listen ? EOS_TRUE : EOS_FALSE };
                api.EOS_RTCAudio_UpdateReceiving(audio, &o, nullptr, OnReceiving);
                room.listen = listen;
                Log(L"voice: hearing the others %s", listen ? L"on" : L"off");
            }
        }

        // Who is talking, by the name the lobby shows (member attribute PD_NAME, the game's gamertag for the slot).
        // Written on the EOS thread, read by the game's lobby drawing (gamepatches.cpp).
        std::mutex g_talkLock;
        std::map<std::string, std::string> g_talkers; // EOS user id -> display name, while speaking
        // Every other participant of a connected voice room, with their mic state (EOS audio status: enabled = their
        // mic is on and sending). The lobby shows the megaphone for players whose mic is on (gamepatches.cpp).
        struct Participant { std::string name, room; bool micOn = false; int heard = -1; /* team channels: played here (1) or not (0) */ };
        std::map<std::string, Participant> g_participants; // EOS user id -> participant, under g_talkLock

        // Tag Team voice channels. In the lobby everyone hears everyone; during a Tag Team match each PC plays only
        // its own team (by the team slot, so a player who switches teams switches channel). Every PC mutes the
        // other team's players on its own side (EOS_RTCAudio_UpdateReceiving per participant): one lobby voice room,
        // no separate rooms (those need a token server).
        std::mutex g_teamLock;
        bool g_teamsActive = false;           // under g_teamLock
        std::map<std::string, int> g_teamOf;  // player name -> team, under g_teamLock
        int g_myTeam = -1;                    // under g_teamLock

        void EOS_CALL OnTeamReceiving(const EOS_RTCAudio_UpdateReceivingCallbackInfo* info)
        {
            if (info->ResultCode != EOS_EResult::EOS_Success) Log(L"voice: team channel switch failed: %s", Result(*Sdk(), info->ResultCode).c_str());
        }

        // EOS thread: plays or mutes each participant as the team channels say.
        void ApplyTeamChannels(const Api& api, EOS_ProductUserId me)
        {
            EOS_HRTCAudio audio = api.EOS_RTC_GetAudioInterface(api.EOS_Platform_GetRTCInterface(g_voicePlatform));
            if (!audio) return;
            bool active;
            std::map<std::string, int> teamOf;
            int myTeam;
            {
                std::lock_guard guard(g_teamLock);
                active = g_teamsActive;
                teamOf = g_teamOf;
                myTeam = g_myTeam;
            }
            std::lock_guard guard(g_talkLock);
            for (auto& [id, p] : g_participants)
            {
                auto team = teamOf.find(p.name);
                int heard = active && team != teamOf.end() && team->second != myTeam ? 0 : 1; // unknown name: keep hearing
                if (p.heard == heard || p.room.empty()) continue;
                EOS_RTCAudio_UpdateReceivingOptions o{ EOS_RTCAUDIO_UPDATERECEIVING_API_LATEST, me, p.room.c_str(), api.EOS_ProductUserId_FromString(id.c_str()), heard ? EOS_TRUE : EOS_FALSE };
                api.EOS_RTCAudio_UpdateReceiving(audio, &o, nullptr, OnTeamReceiving);
                if (p.heard != -1 || !heard) Log(L"voice: team channel: %S %s", p.name.empty() ? id.c_str() : p.name.c_str(), heard ? L"heard" : L"muted (other team)");
                p.heard = heard;
            }
        }

        // The display name of a voice room participant, from the lobby that owns the room.
        std::string ParticipantName(const Api& api, EOS_ProductUserId me, const std::string& roomName, EOS_ProductUserId participant)
        {
            for (auto& [lobbyId, room] : g_voiceRooms)
            {
                if (room.name != roomName) continue;
                Context c{ api, g_voicePlatform, g_lobbyHandle, me };
                if (auto lobby = Local(c, lobbyId))
                    for (auto& m : lobby->members)
                        if (m.userId == Narrow(api, participant))
                        {
                            auto name = m.attributes.find("PD_NAME");
                            return name != m.attributes.end() ? name->second : std::string();
                        }
            }
            return {};
        }

        void EOS_CALL OnSpeaking(const EOS_RTCAudio_ParticipantUpdatedCallbackInfo* info)
        {
            const Api& api = *Sdk();
            std::string id = Narrow(api, info->ParticipantId);
            std::string room = info->RoomName ? info->RoomName : "";
            std::string name = ParticipantName(api, info->LocalUserId, room, info->ParticipantId);
            std::lock_guard guard(g_talkLock);
            if (id != Narrow(api, info->LocalUserId))
            {
                Participant& p = g_participants[id];
                bool micOn = info->AudioStatus == EOS_ERTCAudioStatus::EOS_RTCAS_Enabled;
                if (!name.empty()) p.name = name;
                p.room = room;
                if (p.micOn != micOn) Log(L"voice: %S's mic is %s", p.name.empty() ? id.c_str() : p.name.c_str(), micOn ? L"on" : L"off");
                p.micOn = micOn;
            }
            if (!info->bSpeaking) name.clear();
            auto was = g_talkers.find(id);
            if (was != g_talkers.end() && !info->bSpeaking) name = was->second; // for the log line
            bool wasTalking = was != g_talkers.end();
            if (info->bSpeaking && !name.empty()) g_talkers[id] = name;
            else g_talkers.erase(id);
            if (wasTalking != (g_talkers.count(id) != 0)) Log(L"voice: %S %s talking", name.empty() ? id.c_str() : name.c_str(), info->bSpeaking ? L"started" : L"stopped");
        }

        // This PC's own talking: EOS reports speaking for the other participants, so the local indicator comes from
        // the mic audio actually being sent (EOS's audio thread): loud enough = talking, for the next 400 ms.
        std::string g_myName;                         // under g_talkLock
        std::atomic<uint64_t> g_localTalkUntil{ 0 };
        constexpr int kTalkLevel = 1200;              // peak sample (of 32767)

        void EOS_CALL OnAudioBeforeSend(const EOS_RTCAudio_AudioBeforeSendCallbackInfo* info)
        {
            const EOS_RTCAudio_AudioBuffer* b = info->Buffer;
            if (!b || !b->Frames) return;
            int peak = 0;
            for (uint32_t i = 0; i < b->FramesCount * (std::max)(1u, b->Channels); ++i) peak = (std::max)(peak, std::abs(static_cast<int>(b->Frames[i])));
            if (peak >= kTalkLevel) g_localTalkUntil = GetTickCount64() + 400;
            if (GetConfig().voiceDebug)
            {
                static uint64_t next = 0;
                static int buffers = 0, loudest = 0;
                ++buffers;
                loudest = (std::max)(loudest, peak);
                if (GetTickCount64() >= next)
                {
                    Log(L"voice-debug: mic audio %d buffers (%u Hz, %u ch), loudest %d (talking from %d)", buffers, b->SampleRate, b->Channels, loudest, kTalkLevel);
                    buffers = loudest = 0;
                    next = GetTickCount64() + 3000;
                }
            }
        }

        void EOS_CALL OnInputDevices(const EOS_RTCAudio_OnQueryInputDevicesInformationCallbackInfo* info)
        {
            const Api& api = *Sdk();
            EOS_HRTCAudio audio = api.EOS_RTC_GetAudioInterface(api.EOS_Platform_GetRTCInterface(g_voicePlatform));
            EOS_RTCAudio_GetInputDevicesCountOptions co{ EOS_RTCAUDIO_GETINPUTDEVICESCOUNT_API_LATEST };
            uint32_t n = info->ResultCode == EOS_EResult::EOS_Success ? api.EOS_RTCAudio_GetInputDevicesCount(audio, &co) : 0;
            Log(L"voice-debug: input devices: %s, %u found", Result(api, info->ResultCode).c_str(), n);
            for (uint32_t i = 0; i < n; ++i)
            {
                EOS_RTCAudio_CopyInputDeviceInformationByIndexOptions o{ EOS_RTCAUDIO_COPYINPUTDEVICEINFORMATIONBYINDEX_API_LATEST, i };
                EOS_RTCAudio_InputDeviceInformation* d = nullptr;
                if (api.EOS_RTCAudio_CopyInputDeviceInformationByIndex(audio, &o, &d) != EOS_EResult::EOS_Success || !d) continue;
                Log(L"voice-debug:   %s%s", Wide(d->DeviceName).c_str(), d->bDefaultDevice ? L" (default)" : L"");
                api.EOS_RTCAudio_InputDeviceInformation_Release(d);
            }
        }

        void EOS_CALL OnAudioInputState(const EOS_RTCAudio_AudioInputStateCallbackInfo* info)
        {
            static const wchar_t* names[] = { L"idle", L"recording", L"recording (silent)", L"recording, device disconnected", L"FAILED (no microphone access?)" };
            int s = static_cast<int>(info->Status);
            Log(L"voice: microphone %s", s >= 0 && s < 5 ? names[s] : L"in an unknown state");
        }

        void EOS_CALL OnParticipant(const EOS_RTC_ParticipantStatusChangedCallbackInfo* info)
        {
            Log(L"voice: %s %s the voice room", UserId(*Sdk(), info->ParticipantId).c_str(), info->ParticipantStatus == EOS_ERTCParticipantStatus::EOS_RTCPS_Joined ? L"joined" : L"left");
            const Api& api = *Sdk();
            std::string id = Narrow(api, info->ParticipantId);
            if (info->ParticipantStatus != EOS_ERTCParticipantStatus::EOS_RTCPS_Joined)
            {
                std::lock_guard guard(g_talkLock);
                g_talkers.erase(id);
                g_participants.erase(id);
            }
            else if (id != Narrow(api, info->LocalUserId))
            {
                std::string name = ParticipantName(api, info->LocalUserId, info->RoomName ? info->RoomName : "", info->ParticipantId);
                std::lock_guard guard(g_talkLock);
                Participant& p = g_participants[id];
                if (!name.empty()) p.name = name;
                p.room = info->RoomName ? info->RoomName : "";
            }
        }

        void EOS_CALL OnVoiceRoomConnection(const EOS_Lobby_RTCRoomConnectionChangedCallbackInfo* info)
        {
            const Api& api = *Sdk();
            std::string lobbyId = info->LobbyId ? info->LobbyId : "";
            EOS_HRTC rtc = api.EOS_Platform_GetRTCInterface(g_voicePlatform);
            auto found = g_voiceRooms.find(lobbyId);
            EOS_HRTCAudio audio = api.EOS_RTC_GetAudioInterface(rtc);
            if (found != g_voiceRooms.end())
            {
                if (found->second.participants != EOS_INVALID_NOTIFICATIONID) api.EOS_RTC_RemoveNotifyParticipantStatusChanged(rtc, found->second.participants);
                if (found->second.speaking != EOS_INVALID_NOTIFICATIONID) api.EOS_RTCAudio_RemoveNotifyParticipantUpdated(audio, found->second.speaking);
                if (found->second.sending != EOS_INVALID_NOTIFICATIONID) api.EOS_RTCAudio_RemoveNotifyAudioBeforeSend(audio, found->second.sending);
                if (found->second.input != EOS_INVALID_NOTIFICATIONID) api.EOS_RTCAudio_RemoveNotifyAudioInputState(audio, found->second.input);
                g_localTalkUntil = 0;
                g_voiceRooms.erase(found);
                std::lock_guard guard(g_talkLock);
                g_talkers.clear();
                g_participants.clear();
                g_myName.clear();
            }
            if (!info->bIsConnected)
            {
                Log(L"voice: left the voice room of lobby %S (%s)", lobbyId.c_str(), Result(api, info->DisconnectReason).c_str());
                return;
            }
            char name[256] = {};
            uint32_t len = sizeof(name);
            EOS_Lobby_GetRTCRoomNameOptions o{ EOS_LOBBY_GETRTCROOMNAME_API_LATEST, lobbyId.c_str(), info->LocalUserId };
            EOS_EResult r = api.EOS_Lobby_GetRTCRoomName(g_lobbyHandle, &o, name, &len);
            if (r != EOS_EResult::EOS_Success)
            {
                Log(L"voice: connected to lobby %S's voice room but its name is unavailable: %s", lobbyId.c_str(), Result(api, r).c_str());
                return;
            }
            VoiceRoom& room = g_voiceRooms[lobbyId];
            room.name = name;
            EOS_RTC_AddNotifyParticipantStatusChangedOptions po{ EOS_RTC_ADDNOTIFYPARTICIPANTSTATUSCHANGED_API_LATEST, info->LocalUserId, room.name.c_str() };
            room.participants = api.EOS_RTC_AddNotifyParticipantStatusChanged(rtc, &po, nullptr, OnParticipant);
            EOS_RTCAudio_AddNotifyParticipantUpdatedOptions so{ EOS_RTCAUDIO_ADDNOTIFYPARTICIPANTUPDATED_API_LATEST, info->LocalUserId, room.name.c_str() };
            room.speaking = api.EOS_RTCAudio_AddNotifyParticipantUpdated(audio, &so, nullptr, OnSpeaking);
            EOS_RTCAudio_AddNotifyAudioBeforeSendOptions bo{ EOS_RTCAUDIO_ADDNOTIFYAUDIOBEFORESEND_API_LATEST, info->LocalUserId, room.name.c_str() };
            room.sending = api.EOS_RTCAudio_AddNotifyAudioBeforeSend(audio, &bo, nullptr, OnAudioBeforeSend);
            EOS_RTCAudio_AddNotifyAudioInputStateOptions io{ EOS_RTCAUDIO_ADDNOTIFYAUDIOINPUTSTATE_API_LATEST, info->LocalUserId, room.name.c_str() };
            room.input = api.EOS_RTCAudio_AddNotifyAudioInputState(audio, &io, nullptr, OnAudioInputState);
            if (GetConfig().voiceDebug)
            {
                Log(L"voice-debug: notifications speaking %s, mic audio %s, mic state %s", room.speaking != EOS_INVALID_NOTIFICATIONID ? L"ok" : L"FAILED",
                    room.sending != EOS_INVALID_NOTIFICATIONID ? L"ok" : L"FAILED", room.input != EOS_INVALID_NOTIFICATIONID ? L"ok" : L"FAILED");
                EOS_RTCAudio_QueryInputDevicesInformationOptions qo{ EOS_RTCAUDIO_QUERYINPUTDEVICESINFORMATION_API_LATEST };
                api.EOS_RTCAudio_QueryInputDevicesInformation(audio, &qo, nullptr, OnInputDevices);
            }
            {
                std::string me = ParticipantName(api, info->LocalUserId, room.name, info->LocalUserId);
                std::lock_guard guard(g_talkLock);
                g_myName = me;
                Log(L"voice: talking indicator for '%S' and the other members' PD_NAME", me.c_str());
            }
            Log(L"voice: connected to the voice room of lobby %S", lobbyId.c_str());
            ApplyVoice(api, info->LocalUserId, room);
        }

        // Watches the two switches and passes changes to the connected rooms (on the EOS thread).
        void StartVoiceWatcher()
        {
            static std::once_flag once;
            std::call_once(once, []
            {
                std::thread([]
                {
                    for (int tick = 0;; ++tick)
                    {
                        // Every 2 s: names of participants whose lobby name (PD_NAME) wasn't there yet.
                        if (tick % 8 == 0)
                        {
                            std::string userId = winrt::to_string(eos::LocalUserId());
                            eos::Post([userId](void*)
                            {
                                const Api* api = Sdk();
                                if (!api) return;
                                EOS_ProductUserId me = api->EOS_ProductUserId_FromString(userId.c_str());
                                std::vector<std::pair<std::string, std::string>> unnamed; // id, room
                                {
                                    std::lock_guard guard(g_talkLock);
                                    for (auto& [id, p] : g_participants) if (p.name.empty()) unnamed.emplace_back(id, p.room);
                                }
                                for (auto& [id, room] : unnamed)
                                {
                                    std::string name = ParticipantName(*api, me, room, api->EOS_ProductUserId_FromString(id.c_str()));
                                    if (name.empty()) continue;
                                    std::lock_guard guard(g_talkLock);
                                    if (auto it = g_participants.find(id); it != g_participants.end()) it->second.name = name;
                                }
                            });
                        }
                        // voice_mode=classic: the game's option switches both ways; mic (default): only the microphone.
                        // Team channels: who this PC plays, rechecked every second and at once when the teams change.
                        {
                            std::map<std::string, int> teamOf;
                            int myTeam = -1;
                            bool active = gamepatches::TagTeamMatch(teamOf, myTeam);
                            bool changed;
                            {
                                std::lock_guard guard(g_teamLock);
                                changed = active != g_teamsActive || (active && (myTeam != g_myTeam || teamOf != g_teamOf));
                                if (changed && active != g_teamsActive)
                                    Log(L"voice: %s", active ? L"Tag Team match - team channels on (you hear only your team)" : L"team channels off - everyone hears everyone");
                                g_teamsActive = active;
                                g_teamOf = std::move(teamOf);
                                g_myTeam = myTeam;
                            }
                            if (changed || tick % 4 == 0)
                            {
                                std::string userId = winrt::to_string(eos::LocalUserId());
                                eos::Post([userId](void*)
                                {
                                    const Api* api = Sdk();
                                    if (api) ApplyTeamChannels(*api, api->EOS_ProductUserId_FromString(userId.c_str()));
                                });
                            }
                        }
                        bool mic = gamepatches::GameMicOn(), listen = GetConfig().voiceClassic ? mic : GetConfig().voiceListen;
                        if (mic != g_wantMic.exchange(mic) || listen != g_wantListen.exchange(listen))
                        {
                            std::string userId = winrt::to_string(eos::LocalUserId());
                            eos::Post([userId](void*)
                            {
                                const Api* api = Sdk();
                                if (!api) return;
                                EOS_ProductUserId me = api->EOS_ProductUserId_FromString(userId.c_str());
                                for (auto& [id, room] : g_voiceRooms) ApplyVoice(*api, me, room);
                            });
                        }
                        Sleep(250);
                    }
                }).detach();
            });
        }

        // Join settings for a lobby's voice room: joined with the lobby. The microphone always starts open: started
        // muted, EOS never captured it again after sending was switched on. A Mic: Off setting is applied as "sending
        // disabled" the moment the room connects (OnVoiceRoomConnection -> ApplyVoice).
        EOS_Lobby_LocalRTCOptions LocalVoiceOptions()
        {
            EOS_Lobby_LocalRTCOptions o{};
            o.ApiVersion = EOS_LOBBY_LOCALRTCOPTIONS_API_LATEST;
            o.bLocalAudioDeviceInputStartsMuted = EOS_FALSE;
            return o;
        }

        void RegisterNotifications(const Api& api, EOS_HPlatform platform, EOS_HLobby lobby)
        {
            static bool registered = false; // EOS thread only
            if (registered) return;
            registered = true;
            if (GetConfig().voiceChat)
            {
                g_lobbyHandle = lobby;
                g_voicePlatform = platform;
                EOS_Lobby_AddNotifyRTCRoomConnectionChangedOptions v{ EOS_LOBBY_ADDNOTIFYRTCROOMCONNECTIONCHANGED_API_LATEST };
                if (api.EOS_Lobby_AddNotifyRTCRoomConnectionChanged(lobby, &v, nullptr, OnVoiceRoomConnection) == EOS_INVALID_NOTIFICATIONID)
                    Log(L"voice: room notifications FAILED to register");
                StartVoiceWatcher();
            }
            EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions a{ EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST };
            EOS_Lobby_AddNotifyLobbyMemberUpdateReceivedOptions b{ EOS_LOBBY_ADDNOTIFYLOBBYMEMBERUPDATERECEIVED_API_LATEST };
            EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions s{ EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST };
            bool ok = api.EOS_Lobby_AddNotifyLobbyUpdateReceived(lobby, &a, nullptr, OnLobbyUpdate) != EOS_INVALID_NOTIFICATIONID;
            ok &= api.EOS_Lobby_AddNotifyLobbyMemberUpdateReceived(lobby, &b, nullptr, OnMemberUpdate) != EOS_INVALID_NOTIFICATIONID;
            ok &= api.EOS_Lobby_AddNotifyLobbyMemberStatusReceived(lobby, &s, nullptr, OnMemberStatus) != EOS_INVALID_NOTIFICATIONID;
            Log(L"lobby: change notifications %s", ok ? L"registered" : L"FAILED to register");
        }
    }

    bool Available() { return eos::SignedIn(); }
    std::string LocalUserId() { return winrt::to_string(eos::LocalUserId()); }

    bool IsTalking(const char* name)
    {
        std::lock_guard guard(g_talkLock);
        if (!g_myName.empty() && g_myName == name) return GetTickCount64() < g_localTalkUntil;
        for (auto& [id, talker] : g_talkers)
            if (talker == name) return true;
        return false;
    }

    bool IsMicOn(const char* name)
    {
        std::lock_guard guard(g_talkLock);
        if (!g_myName.empty() && g_myName == name) return g_wantMic; // this PC, while in a voice room
        for (auto& [id, p] : g_participants)
            if (p.name == name) return p.micOn;
        return false;
    }

    std::optional<Lobby> Create(const std::string& id, const std::string& bucket, uint32_t maxMembers, const Attributes& lobbyAttributes, const Attributes& memberAttributes)
    {
        return Run<std::optional<Lobby>>(L"create", [=](const Context& c, std::function<void(std::optional<Lobby>)> finish)
        {
            EOS_Lobby_CreateLobbyOptions o{};
            o.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
            o.LocalUserId = c.me;
            o.MaxLobbyMembers = maxMembers ? maxMembers : 4;
            o.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
            o.bPresenceEnabled = EOS_FALSE;
            o.bAllowInvites = EOS_FALSE;
            o.BucketId = bucket.c_str();
            o.bDisableHostMigration = EOS_FALSE;
            EOS_Lobby_LocalRTCOptions voice = LocalVoiceOptions();
            bool voiceOn = GetConfig().voiceChat;
            o.bEnableRTCRoom = voiceOn ? EOS_TRUE : EOS_FALSE;
            o.LocalRTCOptions = voiceOn ? &voice : nullptr;
            o.LobbyId = id.c_str();
            o.bEnableJoinById = EOS_TRUE;
            o.RTCRoomJoinActionType = voiceOn ? EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin : EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_ManualJoin;
            Context ctx = c;
            c.api.EOS_Lobby_CreateLobby(c.lobby, &o, Then<EOS_Lobby_CreateLobbyCallbackInfo>([ctx, id, lobbyAttributes, memberAttributes, finish](const EOS_Lobby_CreateLobbyCallbackInfo* info)
            {
                if (info->ResultCode != EOS_EResult::EOS_Success)
                {
                    Log(L"lobby: create %S failed: %s", id.c_str(), Result(ctx.api, info->ResultCode).c_str());
                    finish(std::nullopt);
                    return;
                }
                Changes lobby{ lobbyAttributes, {} }, member{ memberAttributes, {} };
                Modify(ctx, id, &lobby, member, 0, [ctx, id, finish](bool ok)
                {
                    if (!ok) Log(L"lobby: %S created but its first attributes were not accepted", id.c_str());
                    finish(Local(ctx, id));
                });
            }), Pending<EOS_Lobby_CreateLobbyCallbackInfo>::Thunk);
        });
    }

    std::optional<Lobby> Join(const std::string& id, const Attributes& memberAttributes)
    {
        return Run<std::optional<Lobby>>(L"join", [=](const Context& c, std::function<void(std::optional<Lobby>)> finish)
        {
            if (Local(c, id))
            {
                Modify(c, id, nullptr, Changes{ memberAttributes, {} }, 0, [c, id, finish](bool) { finish(Local(c, id)); });
                return;
            }
            EOS_Lobby_JoinLobbyByIdOptions o{};
            o.ApiVersion = EOS_LOBBY_JOINLOBBYBYID_API_LATEST;
            o.LobbyId = id.c_str();
            o.LocalUserId = c.me;
            o.bPresenceEnabled = EOS_FALSE;
            // A lobby without a voice room (a host with voice_chat=off or an older pdnet) is joined the same way.
            EOS_Lobby_LocalRTCOptions voice = LocalVoiceOptions();
            bool voiceOn = GetConfig().voiceChat;
            o.LocalRTCOptions = voiceOn ? &voice : nullptr;
            o.RTCRoomJoinActionType = voiceOn ? EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin : EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_ManualJoin;
            Context ctx = c;
            c.api.EOS_Lobby_JoinLobbyById(c.lobby, &o, Then<EOS_Lobby_JoinLobbyByIdCallbackInfo>([ctx, id, memberAttributes, finish](const EOS_Lobby_JoinLobbyByIdCallbackInfo* info)
            {
                if (info->ResultCode != EOS_EResult::EOS_Success)
                {
                    Log(L"lobby: join %S failed: %s", id.c_str(), Result(ctx.api, info->ResultCode).c_str());
                    finish(std::nullopt);
                    return;
                }
                Modify(ctx, id, nullptr, Changes{ memberAttributes, {} }, 0, [ctx, id, finish](bool) { finish(Local(ctx, id)); });
            }), Pending<EOS_Lobby_JoinLobbyByIdCallbackInfo>::Thunk);
        });
    }

    bool Update(const std::string& id, const Changes& lobby, const Changes& member, uint32_t maxMembers)
    {
        if (lobby.empty() && member.empty() && !maxMembers) return true;
        return Run<bool>(L"update", [=](const Context& c, std::function<void(bool)> finish)
        {
            auto current = Local(c, id);
            if (!current)
            {
                Log(L"lobby: update %S: not in that lobby", id.c_str());
                finish(false);
                return;
            }
            bool owner = current->ownerId == Narrow(c.api, c.me);
            if (!owner && (!lobby.empty() || maxMembers))
                Log(L"lobby: %S: session-level change from a non-owner dropped (%zu set, %zu removed)", id.c_str(), lobby.set.size(), lobby.remove.size());
            Modify(c, id, owner ? &lobby : nullptr, member, owner ? maxMembers : 0, finish);
        });
    }

    bool Leave(const std::string& id)
    {
        return Run<bool>(L"leave", [=](const Context& c, std::function<void(bool)> finish)
        {
            EOS_Lobby_LeaveLobbyOptions o{ EOS_LOBBY_LEAVELOBBY_API_LATEST, c.me, id.c_str() };
            const Api& api = c.api;
            c.api.EOS_Lobby_LeaveLobby(c.lobby, &o, Then<EOS_Lobby_LeaveLobbyCallbackInfo>([&api, id, finish](const EOS_Lobby_LeaveLobbyCallbackInfo* info)
            {
                bool ok = info->ResultCode == EOS_EResult::EOS_Success || info->ResultCode == EOS_EResult::EOS_NotFound;
                if (!ok) Log(L"lobby: leave %S failed: %s", id.c_str(), Result(api, info->ResultCode).c_str());
                finish(ok);
            }), Pending<EOS_Lobby_LeaveLobbyCallbackInfo>::Thunk);
        });
    }

    bool Destroy(const std::string& id)
    {
        return Run<bool>(L"destroy", [=](const Context& c, std::function<void(bool)> finish)
        {
            EOS_Lobby_DestroyLobbyOptions o{ EOS_LOBBY_DESTROYLOBBY_API_LATEST, c.me, id.c_str() };
            const Api& api = c.api;
            c.api.EOS_Lobby_DestroyLobby(c.lobby, &o, Then<EOS_Lobby_DestroyLobbyCallbackInfo>([&api, id, finish](const EOS_Lobby_DestroyLobbyCallbackInfo* info)
            {
                bool ok = info->ResultCode == EOS_EResult::EOS_Success || info->ResultCode == EOS_EResult::EOS_NotFound;
                if (!ok) Log(L"lobby: destroy %S failed: %s", id.c_str(), Result(api, info->ResultCode).c_str());
                finish(ok);
            }), Pending<EOS_Lobby_DestroyLobbyCallbackInfo>::Thunk);
        });
    }

    bool Promote(const std::string& id, const std::string& userId)
    {
        return Run<bool>(L"promote", [=](const Context& c, std::function<void(bool)> finish)
        {
            EOS_Lobby_PromoteMemberOptions o{ EOS_LOBBY_PROMOTEMEMBER_API_LATEST, id.c_str(), c.me, c.api.EOS_ProductUserId_FromString(userId.c_str()) };
            const Api& api = c.api;
            c.api.EOS_Lobby_PromoteMember(c.lobby, &o, Then<EOS_Lobby_PromoteMemberCallbackInfo>([&api, id, finish](const EOS_Lobby_PromoteMemberCallbackInfo* info)
            {
                if (info->ResultCode != EOS_EResult::EOS_Success) Log(L"lobby: promote in %S failed: %s", id.c_str(), Result(api, info->ResultCode).c_str());
                finish(info->ResultCode == EOS_EResult::EOS_Success);
            }), Pending<EOS_Lobby_PromoteMemberCallbackInfo>::Thunk);
        });
    }

    std::optional<Lobby> Read(const std::string& id)
    {
        return Run<std::optional<Lobby>>(L"read", [=](const Context& c, std::function<void(std::optional<Lobby>)> finish)
        {
            if (auto local = Local(c, id))
            {
                finish(std::move(local));
                return;
            }
            Find(c, nullptr, &id, 1, [finish](std::vector<Lobby> found)
            {
                finish(found.empty() ? std::nullopt : std::optional<Lobby>(std::move(found.front())));
            });
        });
    }

    std::vector<Lobby> Search(const std::string& bucket, uint32_t maxResults)
    {
        return Run<std::vector<Lobby>>(L"search", [=](const Context& c, std::function<void(std::vector<Lobby>)> finish)
        {
            Find(c, &bucket, nullptr, maxResults, finish);
        });
    }

    void SetListener(std::function<void(const std::string&, Event, const std::string&)> listener)
    {
        std::lock_guard guard(g_lock);
        g_listener = std::move(listener);
    }

    void SetSignOutListener(std::function<void()> listener)
    {
        {
            std::lock_guard guard(g_lock);
            g_signOutListener = std::move(listener);
        }
        eos::SetSignInListener([](bool signedIn)
        {
            if (signedIn) return;
            Dispatch([]
            {
                std::function<void()> listener;
                {
                    std::lock_guard guard(g_lock);
                    listener = g_signOutListener;
                }
                if (listener) listener();
            });
        });
    }
}

#endif
