#include "xsapi_pch.h"
#include "mpsd.h"
#include "../src/lobbies.h"
#include "nametrace.h"
#include <winrt/Windows.Data.Json.h>
#include <bcrypt.h>
#include <algorithm>
#include <cwctype>

namespace pdnet::mpsd
{
    namespace json = winrt::Windows::Data::Json;
    using lobbies::Attributes;
    using lobbies::Changes;

    namespace
    {
        constexpr const char* kVersion = "1";
        // Session-level attributes a non-owner may ask the owner to change (identity and numbering stay the owner's).
        const std::set<std::string> kNotRequestable = { "PD_VERSION", "PD_SCID", "PD_TEMPLATE", "PD_NAME", "PD_CORR", "PD_MIDS", "PD_MIDS#", "PD_NEXTMID" };

        std::string U8(const std::wstring& s) { return winrt::to_string(s); }
        std::wstring U16(const std::string& s) { return std::wstring{ winrt::to_hstring(s) }; }

        std::wstring Lower(std::wstring s)
        {
            for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
            return s;
        }

        std::string Attr(const Attributes& a, const char* key)
        {
            auto it = a.find(key);
            return it == a.end() ? std::string() : it->second;
        }

        int64_t Ticks(winrt::Windows::Foundation::DateTime t) { return t.time_since_epoch().count(); }
        winrt::Windows::Foundation::DateTime FromTicks(int64_t ticks) { return winrt::Windows::Foundation::DateTime{ winrt::Windows::Foundation::TimeSpan{ ticks } }; }

        std::string Bucket(const std::wstring& scid, const std::wstring& templateName)
        {
            return std::string("pd") + kVersion + "|" + U8(scid) + "|" + U8(templateName);
        }

        // JSON helpers (Windows.Data.Json). Maps of raw JSON text are stored as JSON strings, so they round-trip exactly.
        std::string StringMapJson(const std::map<std::wstring, std::wstring>& values)
        {
            json::JsonObject o;
            for (auto& [k, v] : values) o.Insert(k, json::JsonValue::CreateStringValue(v));
            return U8(std::wstring{ o.Stringify() });
        }

        std::map<std::wstring, std::wstring> ParseStringMap(const std::string& text)
        {
            std::map<std::wstring, std::wstring> out;
            json::JsonObject o{ nullptr };
            if (text.empty() || !json::JsonObject::TryParse(winrt::to_hstring(text), o)) return out;
            for (auto const& pair : o)
            {
                auto v = pair.Value();
                out[std::wstring{ pair.Key() }] = v.ValueType() == json::JsonValueType::String ? std::wstring{ v.GetString() } : std::wstring{ v.Stringify() };
            }
            return out;
        }

        std::string StringsJson(const std::vector<std::wstring>& values)
        {
            json::JsonArray a;
            for (auto& v : values) a.Append(json::JsonValue::CreateStringValue(v));
            return U8(std::wstring{ a.Stringify() });
        }

        std::vector<std::wstring> ParseStrings(const std::string& text)
        {
            std::vector<std::wstring> out;
            json::JsonArray a{ nullptr };
            if (text.empty() || !json::JsonArray::TryParse(winrt::to_hstring(text), a)) return out;
            for (auto const& v : a)
                if (v.ValueType() == json::JsonValueType::String) out.emplace_back(v.GetString());
            return out;
        }

        std::string SearchJson(const SearchHandle& h)
        {
            json::JsonObject o, numbers, strings;
            for (auto& [k, v] : h.numbers) numbers.Insert(k, json::JsonValue::CreateNumberValue(v));
            for (auto& [k, v] : h.strings) strings.Insert(k, json::JsonValue::CreateStringValue(v));
            json::JsonArray tags;
            for (auto& t : h.tags) tags.Append(json::JsonValue::CreateStringValue(t));
            o.Insert(L"id", json::JsonValue::CreateStringValue(h.id));
            o.Insert(L"tags", tags);
            o.Insert(L"numbers", numbers);
            o.Insert(L"strings", strings);
            o.Insert(L"created", json::JsonValue::CreateStringValue(std::to_wstring(Ticks(h.created))));
            return U8(std::wstring{ o.Stringify() });
        }

        std::optional<SearchHandle> ParseSearch(const std::string& text)
        {
            json::JsonObject o{ nullptr };
            if (text.empty() || !json::JsonObject::TryParse(winrt::to_hstring(text), o)) return std::nullopt;
            SearchHandle h;
            h.id = o.GetNamedString(L"id", L"");
            for (auto const& t : o.GetNamedArray(L"tags", json::JsonArray{}))
                if (t.ValueType() == json::JsonValueType::String) h.tags.emplace_back(t.GetString());
            for (auto const& p : o.GetNamedObject(L"numbers", json::JsonObject{}))
                if (p.Value().ValueType() == json::JsonValueType::Number) h.numbers[std::wstring{ p.Key() }] = p.Value().GetNumber();
            for (auto const& p : o.GetNamedObject(L"strings", json::JsonObject{}))
                if (p.Value().ValueType() == json::JsonValueType::String) h.strings[std::wstring{ p.Key() }] = p.Value().GetString();
            h.created = FromTicks(_wtoi64(o.GetNamedString(L"created", L"0").c_str()));
            return h;
        }

        // ---- SessionDoc <-> lobby attributes (the table in mpsd.h) ----

        Attributes SessionAttributes(const SessionDoc& d)
        {
            Attributes a;
            a["PD_VERSION"] = kVersion;
            a["PD_SCID"] = U8(d.scid);
            a["PD_TEMPLATE"] = U8(d.templateName);
            a["PD_NAME"] = U8(d.sessionName);
            if (!d.correlationId.empty()) a["PD_CORR"] = U8(d.correlationId);
            if (!d.customConstantsJson.empty()) lobbies::Split(a, "PD_CONST", U8(d.customConstantsJson));
            if (!d.customProperties.empty()) lobbies::Split(a, "PD_PROPS", StringMapJson(d.customProperties));
            if (!d.keywords.empty()) lobbies::Split(a, "PD_KEYWORDS", StringsJson(d.keywords));
            if (!d.initiators.empty()) lobbies::Split(a, "PD_INITIATORS", StringsJson(d.initiators));
            a["PD_VIS"] = std::to_string(static_cast<int>(d.visibility));
            a["PD_JOINR"] = std::to_string(static_cast<int>(d.joinRestriction));
            a["PD_READR"] = std::to_string(static_cast<int>(d.readRestriction));
            a["PD_CLOSED"] = d.closed ? "1" : "0";
            if (!d.hostDeviceToken.empty()) a["PD_HOST"] = U8(d.hostDeviceToken);
            if (d.search) lobbies::Split(a, "PD_SEARCH", SearchJson(*d.search));
            json::JsonObject mids;
            for (auto& m : d.members)
                if (!m.reservation && m.memberId) mids.Insert(m.xuid, json::JsonValue::CreateNumberValue(m.memberId));
            lobbies::Split(a, "PD_MIDS", U8(std::wstring{ mids.Stringify() }));
            a["PD_NEXTMID"] = std::to_string(d.nextMemberId);
            return a;
        }

        Attributes MemberAttributes(const MemberDoc& m)
        {
            Attributes a;
            a["PD_XUID"] = U8(m.xuid);
            if (!m.gamertag.empty()) a["PD_NAME"] = U8(m.gamertag);
            if (!m.deviceAddressBase64.empty()) a["PD_ADDR"] = U8(m.deviceAddressBase64);
            if (!m.customConstantsJson.empty()) lobbies::Split(a, "PD_MCONST", U8(m.customConstantsJson));
            if (!m.customProperties.empty()) lobbies::Split(a, "PD_MPROPS", StringMapJson(m.customProperties));
            a["PD_STATUS"] = std::to_string(static_cast<int>(m.status));
            a["PD_INIT"] = m.initializeRequested ? "1" : "0";
            a["PD_JOIN"] = std::to_string(Ticks(m.joinTime));
            return a;
        }

        Changes Diff(const Attributes& before, const Attributes& after)
        {
            Changes c;
            for (auto& [k, v] : after)
            {
                auto it = before.find(k);
                if (it == before.end() || it->second != v) c.set[k] = v;
            }
            for (auto& [k, v] : before)
                if (!after.count(k)) c.remove.push_back(k);
            return c;
        }

        const lobbies::Member* FindLobbyMember(const lobbies::Lobby& lobby, const std::string& userId)
        {
            for (auto& m : lobby.members)
                if (m.userId == userId) return &m;
            return nullptr;
        }

        // Rebuilds a session from its lobby. Local-only fields (change number, subscription, reservations) come from `previous`.
        SessionDoc FromLobby(const lobbies::Lobby& lobby, const SessionDoc* previous)
        {
            const Attributes& a = lobby.attributes;
            SessionDoc d;
            if (previous)
            {
                d.subscribedChanges = previous->subscribedChanges;
                d.changeNumber = previous->changeNumber;
                d.writeStatus = previous->writeStatus;
            }
            d.scid = U16(Attr(a, "PD_SCID"));
            d.templateName = U16(Attr(a, "PD_TEMPLATE"));
            d.sessionName = U16(Attr(a, "PD_NAME"));
            d.correlationId = U16(Attr(a, "PD_CORR"));
            d.maxMembers = lobby.maxMembers ? lobby.maxMembers : 4;
            d.customConstantsJson = U16(lobbies::Joined(a, "PD_CONST"));
            d.customProperties = ParseStringMap(lobbies::Joined(a, "PD_PROPS"));
            d.keywords = ParseStrings(lobbies::Joined(a, "PD_KEYWORDS"));
            d.initiators = ParseStrings(lobbies::Joined(a, "PD_INITIATORS"));
            if (auto v = Attr(a, "PD_VIS"); !v.empty()) d.visibility = static_cast<mp::MultiplayerSessionVisibility>(atoi(v.c_str()));
            if (auto v = Attr(a, "PD_JOINR"); !v.empty()) d.joinRestriction = static_cast<mp::MultiplayerSessionRestriction>(atoi(v.c_str()));
            if (auto v = Attr(a, "PD_READR"); !v.empty()) d.readRestriction = static_cast<mp::MultiplayerSessionRestriction>(atoi(v.c_str()));
            d.closed = Attr(a, "PD_CLOSED") == "1";
            d.hostDeviceToken = U16(Attr(a, "PD_HOST"));
            d.search = ParseSearch(lobbies::Joined(a, "PD_SEARCH"));
            d.shared = lobby.joined;
            d.lobbyMemberCount = lobby.memberCount;

            std::map<std::wstring, uint32_t> ids;
            for (auto& [xuid, id] : ParseStringMap(lobbies::Joined(a, "PD_MIDS"))) ids[xuid] = static_cast<uint32_t>(_wtoi(id.c_str()));
            uint32_t next = static_cast<uint32_t>(atoi(Attr(a, "PD_NEXTMID").c_str()));
            if (!next) next = 1;

            std::vector<MemberDoc> unnumbered;
            for (auto& lm : lobby.members)
            {
                const Attributes& ma = lm.attributes;
                MemberDoc m;
                m.xuid = U16(Attr(ma, "PD_XUID"));
                if (m.xuid.empty()) continue; // joined, attributes not set yet
                m.gamertag = U16(Attr(ma, "PD_NAME"));
                m.deviceAddressBase64 = U16(Attr(ma, "PD_ADDR"));
                m.customConstantsJson = U16(lobbies::Joined(ma, "PD_MCONST"));
                m.customProperties = ParseStringMap(lobbies::Joined(ma, "PD_MPROPS"));
                if (auto v = Attr(ma, "PD_STATUS"); !v.empty()) m.status = static_cast<mp::MultiplayerSessionMemberStatus>(atoi(v.c_str()));
                m.initializeRequested = Attr(ma, "PD_INIT") == "1";
                m.joinTime = FromTicks(_atoi64(Attr(ma, "PD_JOIN").c_str()));
                if (lm.userId == lobby.ownerId) d.ownerXuid = m.xuid;
                if (auto it = ids.find(m.xuid); it != ids.end() && it->second)
                {
                    m.memberId = it->second;
                    next = (std::max)(next, m.memberId + 1);
                    d.members.push_back(std::move(m));
                }
                else
                {
                    unnumbered.push_back(std::move(m));
                }
            }
            // Provisional ids (the owner assigns the real ones in the same order: join time, then xuid).
            std::sort(unnumbered.begin(), unnumbered.end(), [](const MemberDoc& x, const MemberDoc& y)
            {
                return Ticks(x.joinTime) != Ticks(y.joinTime) ? Ticks(x.joinTime) < Ticks(y.joinTime) : x.xuid < y.xuid;
            });
            for (auto& m : unnumbered)
            {
                m.memberId = next++;
                d.members.push_back(std::move(m));
            }
            d.nextMemberId = next;
            if (previous)
                for (auto& r : previous->members)
                    if (r.reservation && !d.FindMember(r.xuid)) d.members.push_back(r);
            std::sort(d.members.begin(), d.members.end(), [](const MemberDoc& x, const MemberDoc& y) { return x.memberId < y.memberId; });
            return d;
        }

        void ApplyEdits(SessionDoc& doc, const SessionDoc& written)
        {
            const Edits& e = written.edits;
            if (e.maxMembers) doc.maxMembers = written.maxMembers;
            if (e.visibility) doc.visibility = written.visibility;
            if (e.joinRestriction) doc.joinRestriction = written.joinRestriction;
            if (e.readRestriction) doc.readRestriction = written.readRestriction;
            if (e.host) doc.hostDeviceToken = written.hostDeviceToken;
            if (e.closed) doc.closed = written.closed;
            if (e.keywords) doc.keywords = written.keywords;
            for (auto& name : e.properties)
            {
                auto it = written.customProperties.find(name);
                if (it == written.customProperties.end()) doc.customProperties.erase(name);
                else doc.customProperties[name] = it->second;
            }
        }

        std::wstring NewCorrelationId()
        {
            std::wstring id = winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid()).c_str();
            return id.substr(1, id.size() - 2); // drop braces
        }

        // PD_REQ: {"seq":"n","set":{key:value},"remove":[key]}
        std::string RequestJson(uint64_t seq, const Changes& c)
        {
            json::JsonObject o, set;
            json::JsonArray remove;
            for (auto& [k, v] : c.set) set.Insert(U16(k), json::JsonValue::CreateStringValue(U16(v)));
            for (auto& k : c.remove) remove.Append(json::JsonValue::CreateStringValue(U16(k)));
            o.Insert(L"seq", json::JsonValue::CreateStringValue(std::to_wstring(seq)));
            o.Insert(L"set", set);
            o.Insert(L"remove", remove);
            return U8(std::wstring{ o.Stringify() });
        }

        bool ParseRequest(const std::string& text, uint64_t& seq, Changes& c)
        {
            json::JsonObject o{ nullptr };
            if (text.empty() || !json::JsonObject::TryParse(winrt::to_hstring(text), o)) return false;
            seq = _wcstoui64(o.GetNamedString(L"seq", L"0").c_str(), nullptr, 10);
            for (auto const& p : o.GetNamedObject(L"set", json::JsonObject{}))
                if (p.Value().ValueType() == json::JsonValueType::String) c.set[U8(std::wstring{ p.Key() })] = U8(std::wstring{ p.Value().GetString() });
            for (auto const& k : o.GetNamedArray(L"remove", json::JsonArray{}))
                if (k.ValueType() == json::JsonValueType::String) c.remove.push_back(U8(std::wstring{ k.GetString() }));
            return true;
        }

        uint64_t NextRequestSeq()
        {
            static std::atomic<uint64_t> last{ 0 };
            // Milliseconds since 1601 keep growing across restarts, so the owner never mistakes a new request for an old one.
            uint64_t now = static_cast<uint64_t>(Ticks(winrt::clock::now()) / 10000);
            uint64_t prev = last.load();
            uint64_t next;
            do { next = (std::max)(now, prev + 1); } while (!last.compare_exchange_weak(prev, next));
            return next;
        }

        bool Matches(const std::wstring& actual, const std::wstring& wanted)
        {
            return wanted.empty() || _wcsicmp(actual.c_str(), wanted.c_str()) == 0;
        }
    }

    std::wstring JsonObject(const std::map<std::wstring, std::wstring>& values)
    {
        std::wstring json = L"{";
        for (auto& [name, value] : values)
        {
            if (json.size() > 1) json += L",";
            json += L"\"" + name + L"\":" + (value.empty() ? L"null" : value);
        }
        return json + L"}";
    }

    std::wstring SessionDoc::SessionPropertiesJson() const
    {
        return JsonObject(customProperties);
    }

    MemberDoc* SessionDoc::FindMember(const std::wstring& xuid)
    {
        for (auto& m : members)
            if (m.xuid == xuid) return &m;
        return nullptr;
    }

    const MemberDoc* SessionDoc::FindMember(const std::wstring& xuid) const
    {
        return const_cast<SessionDoc*>(this)->FindMember(xuid);
    }

    std::wstring LobbyIdFor(const std::wstring& key)
    {
        std::string bytes = U8(key);
        uint8_t hash[32] = {};
        BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), hash, sizeof(hash));
        wchar_t out[4 + 40] = L"pd1";
        for (int i = 0; i < 20; ++i) swprintf_s(out + 3 + i * 2, 3, L"%02x", hash[i]);
        return out;
    }

    mp::MultiplayerSessionChangeTypes Compare(const SessionDoc& after, const SessionDoc& before)
    {
        using T = mp::MultiplayerSessionChangeTypes;
        int changes = 0;
        if (after.hostDeviceToken != before.hostDeviceToken) changes |= static_cast<int>(T::HostDeviceTokenChange);
        if (after.joinRestriction != before.joinRestriction || after.closed != before.closed || after.visibility != before.visibility || after.maxMembers != before.maxMembers)
            changes |= static_cast<int>(T::SessionJoinabilityChange);
        if (after.customProperties != before.customProperties || after.keywords != before.keywords) changes |= static_cast<int>(T::CustomPropertyChange);

        std::set<std::wstring> a, b;
        for (auto& m : after.members) if (!m.leaving) a.insert(m.xuid);
        for (auto& m : before.members) if (!m.leaving) b.insert(m.xuid);
        if (a != b) changes |= static_cast<int>(T::MemberListChange);
        for (auto& m : after.members)
        {
            const MemberDoc* old = before.FindMember(m.xuid);
            if (!old) continue;
            if (old->status != m.status) changes |= static_cast<int>(T::MemberStatusChange);
            if (old->customProperties != m.customProperties) changes |= static_cast<int>(T::MemberCustomPropertyChange);
            if (old->deviceAddressBase64 != m.deviceAddressBase64 || old->memberId != m.memberId) changes |= static_cast<int>(T::MemberListChange);
        }
        return static_cast<T>(changes);
    }

    // ---- search filter: terms "numbers/<k> <op> <n>" / "strings/<k> <op> '<s>'", joined with "and" / "or" ----
    bool MatchesFilter(const SearchHandle& h, const std::wstring& filter)
    {
        std::vector<std::wstring> tokens;
        for (size_t i = 0; i < filter.size();)
        {
            wchar_t c = filter[i];
            if (iswspace(c) || c == L'(' || c == L')') { ++i; continue; }
            if (c == L'\'')
            {
                size_t end = filter.find(L'\'', i + 1);
                if (end == std::wstring::npos) end = filter.size();
                tokens.push_back(filter.substr(i, end - i + 1));
                i = end + 1;
                continue;
            }
            size_t end = i;
            while (end < filter.size() && !iswspace(filter[end]) && filter[end] != L'(' && filter[end] != L')') ++end;
            tokens.push_back(filter.substr(i, end - i));
            i = end;
        }

        auto term = [&](const std::wstring& path, const std::wstring& op, std::wstring value) -> bool
        {
            std::wstring p = Lower(path), o = Lower(op);
            auto cmp = [&](int c)
            {
                if (o == L"eq") return c == 0;
                if (o == L"ne") return c != 0;
                if (o == L"gt") return c > 0;
                if (o == L"ge") return c >= 0;
                if (o == L"lt") return c < 0;
                if (o == L"le") return c <= 0;
                return true;
            };
            if (p.rfind(L"numbers/", 0) == 0)
            {
                std::wstring key = p.substr(8);
                for (auto& [k, v] : h.numbers)
                    if (Lower(k) == key)
                    {
                        double want = _wtof(value.c_str());
                        return cmp(v < want ? -1 : v > want ? 1 : 0);
                    }
                return false;
            }
            if (p.rfind(L"strings/", 0) == 0)
            {
                std::wstring key = p.substr(8);
                if (value.size() >= 2 && value.front() == L'\'') value = value.substr(1, value.size() - 2);
                for (auto& [k, v] : h.strings)
                    if (Lower(k) == key) return cmp(_wcsicmp(v.c_str(), value.c_str()));
                return false;
            }
            static std::once_flag logged;
            std::call_once(logged, [&] { Log(L"xsapi-pdnet: search filter term '%s' not understood, ignored", path.c_str()); });
            return true;
        };

        // or-of-ands
        bool any = false, all = true;
        size_t i = 0;
        while (i + 2 < tokens.size())
        {
            all = term(tokens[i], tokens[i + 1], tokens[i + 2]) && all;
            i += 3;
            if (i < tokens.size() && Lower(tokens[i]) == L"or")
            {
                any = any || all;
                all = true;
                ++i;
            }
            else if (i < tokens.size() && Lower(tokens[i]) == L"and")
            {
                ++i;
            }
        }
        return any || all;
    }

    // ---- store ----

    SessionStore& SessionStore::Instance()
    {
        static SessionStore store;
        return store;
    }

    SessionStore::SessionStore()
    {
        lobbies::SetListener([this](const std::string& lobbyId, lobbies::Event event, const std::string& userId)
        {
            OnLobbyEvent(lobbyId, static_cast<int>(event), userId);
        });
        lobbies::SetSignOutListener([this] { OnSignedOut(); });
        nametrace::Start();
    }

    void SessionStore::SetHandlers(ChangedHandler changed, std::function<void()> subscriptionLost)
    {
        std::lock_guard guard(m_lock);
        m_changed = std::move(changed);
        m_subscriptionLost = std::move(subscriptionLost);
    }

    void SessionStore::Raise(const Raised& raised)
    {
        ChangedHandler handler;
        {
            std::lock_guard guard(m_lock);
            handler = m_changed;
        }
        for (auto& [session, changes] : raised)
        {
            Log(L"xsapi-pdnet: session %s/%s changed remotely (types 0x%X, change %llu, %zu member(s))", session->templateName.c_str(), session->sessionName.c_str(),
                static_cast<int>(changes), session->changeNumber, session->members.size());
            if (handler) handler(session, changes);
        }
    }

    // This PC's changes -> the lobby: session-level ones directly if we own it, else as a PD_REQ for the owner;
    // our own member entry always. Then reload the doc from EOS. Called with m_writeLock held.
    bool SessionStore::Push(SessionDoc& doc)
    {
        std::wstring key = doc.Key();
        std::string lobbyId = U8(LobbyIdFor(key));
        auto live = lobbies::Read(lobbyId);
        if (!live || !live->joined)
        {
            Log(L"xsapi-pdnet: lobby for %s/%s is gone; session is now local to this PC", doc.templateName.c_str(), doc.sessionName.c_str());
            doc.shared = false;
            return false;
        }
        std::string me = lobbies::LocalUserId();
        bool owner = live->ownerId == me;
        const MemberDoc* mine = doc.FindMember(xsapi::LocalXuid());
        const lobbies::Member* liveMine = FindLobbyMember(*live, me);

        Changes session = Diff(live->attributes, SessionAttributes(doc));
        Attributes memberWant = mine ? MemberAttributes(*mine) : Attributes{};
        if (!owner)
        {
            // Keep asking until the owner's copy shows every requested change.
            Changes request;
            uint64_t seq = 0;
            ParseRequest(m_myRequests[key], seq, request);
            Changes before = request;
            for (auto& [k, v] : session.set)
                if (!kNotRequestable.count(k)) request.set[k] = v;
            for (auto& k : session.remove)
                if (!kNotRequestable.count(k))
                {
                    request.set.erase(k);
                    if (std::find(request.remove.begin(), request.remove.end(), k) == request.remove.end()) request.remove.push_back(k);
                }
            std::erase_if(request.set, [&](auto& kv) { return Attr(live->attributes, kv.first.c_str()) == kv.second; });
            std::erase_if(request.remove, [&](auto& k) { return !live->attributes.count(k); });
            if (request.empty())
            {
                m_myRequests.erase(key);
            }
            else
            {
                bool same = seq && before.set == request.set && before.remove == request.remove;
                if (!same) seq = NextRequestSeq();
                m_myRequests[key] = RequestJson(seq, request);
                lobbies::Split(memberWant, "PD_REQ", m_myRequests[key]);
                if (!same) Log(L"xsapi-pdnet: asking the owner of %s/%s to apply %zu session change(s)", doc.templateName.c_str(), doc.sessionName.c_str(), request.set.size() + request.remove.size());
            }
            session = Changes{};
        }
        Changes member = Diff(liveMine ? liveMine->attributes : Attributes{}, memberWant);
        uint32_t maxMembers = owner && doc.maxMembers != live->maxMembers ? doc.maxMembers : 0;
        bool ok = lobbies::Update(lobbyId, session, member, maxMembers);
        if (owner) OwnerDuties(lobbyId);

        if (auto fresh = lobbies::Read(lobbyId); fresh && fresh->joined)
        {
            doc = FromLobby(*fresh, &doc);
            return ok;
        }
        doc.shared = false;
        return false;
    }

    // As lobby owner: number new members, apply other members' PD_REQ, and make the game's host the lobby owner.
    // Returns true if it changed the lobby. Called with m_writeLock held.
    bool SessionStore::OwnerDuties(const std::string& lobbyId)
    {
        auto lobby = lobbies::Read(lobbyId);
        std::string me = lobbies::LocalUserId();
        if (!lobby || !lobby->joined || lobby->ownerId != me) return false;
        const Attributes& a = lobby->attributes;
        Changes changes;

        for (auto& m : lobby->members)
        {
            if (m.userId == me) continue;
            uint64_t seq = 0;
            Changes request;
            if (!ParseRequest(lobbies::Joined(m.attributes, "PD_REQ"), seq, request)) continue;
            uint64_t& applied = m_requestsApplied[lobbyId + "|" + m.userId];
            if (seq <= applied) continue;
            applied = seq;
            for (auto& [k, v] : request.set)
                if (!kNotRequestable.count(k)) changes.set[k] = v;
            for (auto& k : request.remove)
                if (!kNotRequestable.count(k) && a.count(k)) changes.remove.push_back(k);
            Log(L"xsapi-pdnet: applying %zu session change(s) asked for by member %S", request.set.size() + request.remove.size(), m.userId.c_str());
        }

        // Member numbering in join order.
        std::map<std::wstring, std::wstring> ids = ParseStringMap(lobbies::Joined(a, "PD_MIDS"));
        uint32_t next = static_cast<uint32_t>(atoi(Attr(a, "PD_NEXTMID").c_str()));
        if (!next) next = 1;
        for (auto& [x, id] : ids) next = (std::max)(next, static_cast<uint32_t>(_wtoi(id.c_str())) + 1);
        std::vector<std::pair<int64_t, std::wstring>> unnumbered;
        std::set<std::wstring> present;
        for (auto& m : lobby->members)
        {
            std::wstring xuid = U16(Attr(m.attributes, "PD_XUID"));
            if (xuid.empty()) continue;
            present.insert(xuid);
            if (!ids.count(xuid)) unnumbered.emplace_back(_atoi64(Attr(m.attributes, "PD_JOIN").c_str()), xuid);
        }
        bool renumber = std::erase_if(ids, [&](auto& kv) { return !present.count(kv.first); }) > 0;
        std::sort(unnumbered.begin(), unnumbered.end());
        for (auto& [t, xuid] : unnumbered) ids[xuid] = std::to_wstring(next++);
        if (renumber || !unnumbered.empty())
        {
            json::JsonObject o;
            for (auto& [x, id] : ids) o.Insert(x, json::JsonValue::CreateNumberValue(_wtoi(id.c_str())));
            Attributes mids;
            lobbies::Split(mids, "PD_MIDS", U8(std::wstring{ o.Stringify() }));
            mids["PD_NEXTMID"] = std::to_string(next);
            for (auto& k : lobbies::SplitKeys(a, "PD_MIDS"))
                if (!mids.count(k)) changes.remove.push_back(k);
            for (auto& [k, v] : mids) changes.set[k] = v;
        }

        bool changed = false;
        if (!changes.empty()) changed = lobbies::Update(lobbyId, changes, Changes{});

        // The game's host (PD_HOST is the host's xuid) should own the lobby, so it can write the session.
        std::string host = changes.set.count("PD_HOST") ? changes.set["PD_HOST"] : Attr(a, "PD_HOST");
        for (auto& m : lobby->members)
            if (m.userId != me && !host.empty() && Attr(m.attributes, "PD_XUID") == host)
            {
                Log(L"xsapi-pdnet: game host moved to member %S; handing the lobby over", m.userId.c_str());
                lobbies::Promote(lobbyId, m.userId);
                changed = true;
            }
        return changed;
    }

    SessionPtr SessionStore::Write(const SessionDoc& written, mp::MultiplayerSessionWriteMode mode, bool& created, bool& deleted, mp::WriteSessionStatus& status)
    {
        std::lock_guard writing(m_writeLock);
        created = deleted = false;
        status = mp::WriteSessionStatus::Unknown;
        std::wstring key = written.Key();
        const std::wstring& myXuid = xsapi::LocalXuid();
        bool eos = lobbies::Available();

        std::optional<SessionDoc> current;
        {
            std::lock_guard guard(m_lock);
            if (auto it = m_sessions.find(key); it != m_sessions.end()) current = it->second;
        }
        bool remote = false;
        if (!current && eos && mode != mp::MultiplayerSessionWriteMode::CreateNew)
        {
            if (auto lobby = lobbies::Read(U8(LobbyIdFor(key))))
            {
                current = FromLobby(*lobby, nullptr);
                remote = true;
            }
        }
        bool exists = current.has_value();
        if (mode == mp::MultiplayerSessionWriteMode::CreateNew && exists)
        {
            status = mp::WriteSessionStatus::Conflict;
            return nullptr;
        }
        if ((mode == mp::MultiplayerSessionWriteMode::UpdateExisting || mode == mp::MultiplayerSessionWriteMode::SynchronizedUpdate) && !exists)
        {
            status = mp::WriteSessionStatus::HandleNotFound;
            return nullptr;
        }

        SessionDoc doc;
        if (!exists)
        {
            doc = written;
            doc.members.clear();
            doc.nextMemberId = 1;
            doc.correlationId = NewCorrelationId();
            doc.shared = false;
            doc.ownerXuid.clear();
            created = true;
        }
        else
        {
            doc = std::move(*current);
            ApplyEdits(doc, written);
        }
        if (written.subscribedChanges != mp::MultiplayerSessionChangeTypes::None) doc.subscribedChanges = written.subscribedChanges;
        doc.edits = {};

        // Members: this PC's own entry, plus reservations it made (kept on this PC only).
        if (const MemberDoc* me = written.FindMember(myXuid))
        {
            if (me->leaving)
            {
                std::erase_if(doc.members, [&](const MemberDoc& m) { return m.xuid == myXuid; });
            }
            else if (MemberDoc* existing = doc.FindMember(myXuid))
            {
                uint32_t id = existing->memberId;
                auto joined = existing->joinTime;
                *existing = *me;
                existing->memberId = id;
                existing->joinTime = joined;
                existing->reservation = false;
            }
            else
            {
                MemberDoc added = *me;
                added.memberId = doc.nextMemberId++;
                added.joinTime = winrt::clock::now();
                added.reservation = false;
                doc.members.push_back(added);
            }
        }
        for (const MemberDoc& m : written.members)
            if (m.reservation && !doc.FindMember(m.xuid))
            {
                MemberDoc r = m;
                r.memberId = doc.nextMemberId++;
                doc.members.push_back(r);
                Log(L"xsapi-pdnet: reservation for %s kept on this PC only", m.xuid.c_str());
            }

        // EOS: create, join, update or leave the lobby.
        MemberDoc* mine = doc.FindMember(myXuid);
        bool left = false;
        if (eos)
        {
            std::string lobbyId = U8(LobbyIdFor(key));
            if (mine && !doc.shared && (created || remote))
            {
                std::optional<lobbies::Lobby> lobby;
                if (created)
                {
                    lobby = lobbies::Create(lobbyId, Bucket(doc.scid, doc.templateName), doc.maxMembers, SessionAttributes(doc), MemberAttributes(*mine));
                    if (!lobby) lobby = lobbies::Join(lobbyId, MemberAttributes(*mine)); // the id is taken: someone made it first
                    if (!lobby) Log(L"xsapi-pdnet: could not create the lobby for %s/%s; the session is local to this PC", doc.templateName.c_str(), doc.sessionName.c_str());
                    else Log(L"xsapi-pdnet: lobby %S created for %s/%s", lobbyId.c_str(), doc.templateName.c_str(), doc.sessionName.c_str());
                }
                else
                {
                    lobby = lobbies::Join(lobbyId, MemberAttributes(*mine));
                    if (!lobby && remote)
                    {
                        status = doc.members.size() >= doc.maxMembers ? mp::WriteSessionStatus::Conflict : mp::WriteSessionStatus::HandleNotFound;
                        Log(L"xsapi-pdnet: could not join %s/%s (lobby %S)", doc.templateName.c_str(), doc.sessionName.c_str(), lobbyId.c_str());
                        return nullptr;
                    }
                    if (lobby) Log(L"xsapi-pdnet: joined lobby %S (%s/%s, %zu member(s))", lobbyId.c_str(), doc.templateName.c_str(), doc.sessionName.c_str(), lobby->members.size());
                }
                if (lobby && lobby->joined)
                {
                    OwnerDuties(lobbyId);
                    if (auto fresh = lobbies::Read(lobbyId); fresh && fresh->joined) lobby = fresh;
                    doc = FromLobby(*lobby, &doc);
                    if (!created && written.edits.any())
                    {
                        // Session-level edits made in the same write as the join.
                        ApplyEdits(doc, written);
                        Push(doc);
                    }
                }
            }
            else if (mine && doc.shared)
            {
                Push(doc);
            }
            else if (!mine && doc.shared)
            {
                auto live = lobbies::Read(lobbyId);
                bool alone = live && live->members.size() <= 1;
                if (live && live->joined && live->ownerId == lobbies::LocalUserId() && alone) lobbies::Destroy(lobbyId);
                else lobbies::Leave(lobbyId);
                Log(L"xsapi-pdnet: left lobby %S (%s/%s)%s", lobbyId.c_str(), doc.templateName.c_str(), doc.sessionName.c_str(), alone ? L", destroyed" : L"");
                doc.shared = false;
                m_myRequests.erase(key);
                left = true;
            }
        }

        doc.changeNumber++;
        doc.writeStatus = created ? mp::WriteSessionStatus::Created : mp::WriteSessionStatus::Updated;
        status = doc.writeStatus;
        auto snapshot = std::make_shared<SessionDoc>(doc);
        {
            std::lock_guard guard(m_lock);
            bool keep = !doc.members.empty() && !left;
            if (keep) m_sessions[key] = std::move(doc);
            else m_sessions.erase(key);
            deleted = !keep;
        }
        return snapshot;
    }

    SessionPtr SessionStore::Get(const std::wstring& key)
    {
        {
            std::lock_guard guard(m_lock);
            if (auto it = m_sessions.find(key); it != m_sessions.end()) return std::make_shared<SessionDoc>(it->second);
        }
        if (!lobbies::Available()) return nullptr;
        auto lobby = lobbies::Read(U8(LobbyIdFor(key)));
        return lobby ? std::make_shared<SessionDoc>(FromLobby(*lobby, nullptr)) : nullptr;
    }

    SessionPtr SessionStore::GetByHandle(const std::wstring& handleOrLobbyId)
    {
        {
            std::lock_guard guard(m_lock);
            for (auto& [key, doc] : m_sessions)
                if (LobbyIdFor(key) == handleOrLobbyId || (doc.search && doc.search->id == handleOrLobbyId)) return std::make_shared<SessionDoc>(doc);
        }
        if (!lobbies::Available()) return nullptr;
        auto lobby = lobbies::Read(U8(handleOrLobbyId));
        return lobby ? std::make_shared<SessionDoc>(FromLobby(*lobby, nullptr)) : nullptr;
    }

    std::vector<SessionPtr> SessionStore::All()
    {
        std::lock_guard guard(m_lock);
        std::vector<SessionPtr> out;
        for (auto& [key, doc] : m_sessions) out.push_back(std::make_shared<SessionDoc>(doc));
        return out;
    }

    bool SessionStore::SetSearchHandle(const std::wstring& key, SearchHandle handle)
    {
        std::lock_guard writing(m_writeLock);
        SessionDoc doc;
        {
            std::lock_guard guard(m_lock);
            auto it = m_sessions.find(key);
            if (it == m_sessions.end()) return false;
            doc = it->second;
        }
        handle.id = LobbyIdFor(key);
        if (doc.search) handle.created = doc.search->created;
        if (!Ticks(handle.created)) handle.created = winrt::clock::now();
        doc.search = std::move(handle);
        if (doc.shared && lobbies::Available()) Push(doc);
        std::lock_guard guard(m_lock);
        if (auto it = m_sessions.find(key); it != m_sessions.end()) it->second = std::move(doc);
        return true;
    }

    bool SessionStore::ClearSearchHandle(const std::wstring& handleId)
    {
        std::lock_guard writing(m_writeLock);
        SessionDoc doc;
        {
            std::lock_guard guard(m_lock);
            auto it = std::find_if(m_sessions.begin(), m_sessions.end(), [&](auto& kv) { return kv.second.search && kv.second.search->id == handleId; });
            if (it == m_sessions.end()) return false;
            doc = it->second;
        }
        doc.search.reset();
        if (doc.shared && lobbies::Available()) Push(doc);
        std::lock_guard guard(m_lock);
        if (auto it = m_sessions.find(doc.Key()); it != m_sessions.end()) it->second = std::move(doc);
        return true;
    }

    std::vector<SessionPtr> SessionStore::SearchHandles(const std::wstring& scid, const std::wstring& templateName)
    {
        std::vector<SessionPtr> out;
        std::set<std::wstring> seen;
        if (lobbies::Available())
        {
            for (auto& lobby : lobbies::Search(Bucket(scid, templateName)))
            {
                SessionDoc doc = FromLobby(lobby, nullptr);
                if (!doc.search || !Matches(doc.scid, scid) || !Matches(doc.templateName, templateName)) continue;
                seen.insert(doc.Key());
                out.push_back(std::make_shared<SessionDoc>(std::move(doc)));
            }
        }
        std::lock_guard guard(m_lock);
        for (auto& [key, doc] : m_sessions)
            if (doc.search && !seen.count(key) && Matches(doc.scid, scid) && Matches(doc.templateName, templateName))
                out.push_back(std::make_shared<SessionDoc>(doc));
        return out;
    }

    void SessionStore::OnLobbyEvent(const std::string& lobbyId, int event, const std::string& userId)
    {
        Raised raised;
        {
            std::lock_guard writing(m_writeLock);
            std::optional<SessionDoc> old;
            {
                std::lock_guard guard(m_lock);
                for (auto& [key, doc] : m_sessions)
                    if (doc.shared && U8(LobbyIdFor(key)) == lobbyId) old = doc;
            }
            if (!old) return;

            auto e = static_cast<lobbies::Event>(event);
            std::string me = lobbies::LocalUserId();
            bool lost = e == lobbies::Event::Closed || (e == lobbies::Event::Kicked && userId == me);
            std::optional<lobbies::Lobby> lobby;
            if (!lost)
            {
                OwnerDuties(lobbyId); // no-op unless this PC owns the lobby
                lobby = lobbies::Read(lobbyId);
                lost = !lobby || !lobby->joined;
            }

            SessionDoc doc;
            if (lost)
            {
                doc = *old;
                std::erase_if(doc.members, [&](const MemberDoc& m) { return m.xuid == xsapi::LocalXuid(); });
                doc.shared = false;
                Log(L"xsapi-pdnet: lobby %S closed or this PC was removed; %s/%s is no longer shared", lobbyId.c_str(), doc.templateName.c_str(), doc.sessionName.c_str());
            }
            else
            {
                doc = FromLobby(*lobby, &*old);
            }

            auto changes = Compare(doc, *old);
            if (lost) changes = static_cast<mp::MultiplayerSessionChangeTypes>(static_cast<int>(changes) | static_cast<int>(mp::MultiplayerSessionChangeTypes::MemberListChange));
            if (changes == mp::MultiplayerSessionChangeTypes::None) return;
            doc.changeNumber = old->changeNumber + 1;
            auto snapshot = std::make_shared<SessionDoc>(doc);
            {
                std::lock_guard guard(m_lock);
                m_sessions[doc.Key()] = std::move(doc);
            }
            raised.emplace_back(snapshot, changes);
        }
        Raise(raised);
    }

    void SessionStore::OnSignedOut()
    {
        std::function<void()> lost;
        size_t count = 0;
        {
            std::lock_guard writing(m_writeLock);
            std::lock_guard guard(m_lock);
            for (auto& [key, doc] : m_sessions)
                if (doc.shared)
                {
                    doc.shared = false;
                    ++count;
                }
            lost = m_subscriptionLost;
        }
        Log(L"xsapi-pdnet: EOS sign-in lost; %zu shared session(s) are now local only", count);
        if (count && lost) lost();
    }
}
