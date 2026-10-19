#include "xsapi_pch.h"
#include "mpsd.h"

namespace pdnet::mpsd
{
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

    SessionStore& SessionStore::Instance()
    {
        static SessionStore store;
        return store;
    }

    SessionPtr SessionStore::Write(const SessionDoc& written, mp::MultiplayerSessionWriteMode mode, bool& created, bool& deleted)
    {
        std::lock_guard guard(m_lock);
        created = deleted = false;
        auto it = m_sessions.find(written.Key());
        bool exists = it != m_sessions.end();
        if ((mode == mp::MultiplayerSessionWriteMode::CreateNew && exists) ||
            ((mode == mp::MultiplayerSessionWriteMode::UpdateExisting || mode == mp::MultiplayerSessionWriteMode::SynchronizedUpdate) && !exists))
        {
            return nullptr;
        }

        SessionDoc& stored = exists ? it->second : m_sessions[written.Key()];
        if (!exists)
        {
            stored = written;
            stored.members.clear();
            stored.nextMemberId = 1;
            stored.correlationId = winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid()).c_str();
            stored.correlationId = stored.correlationId.substr(1, stored.correlationId.size() - 2); // drop braces
            created = true;
        }
        else
        {
            // Session-level fields: the writer's copy wins (Stage A has a single writer per session).
            auto members = std::move(stored.members);
            auto nextId = stored.nextMemberId;
            auto correlation = stored.correlationId;
            auto change = stored.changeNumber;
            stored = written;
            stored.members = std::move(members);
            stored.nextMemberId = nextId;
            stored.correlationId = correlation;
            stored.changeNumber = change;
        }

        // Members: apply each written member (joins, updates, leaves) onto the stored list.
        for (const MemberDoc& m : written.members)
        {
            MemberDoc* existing = stored.FindMember(m.xuid);
            if (m.leaving)
            {
                std::erase_if(stored.members, [&](const MemberDoc& s) { return s.xuid == m.xuid; });
                continue;
            }
            if (existing)
            {
                uint32_t id = existing->memberId;
                auto joined = existing->joinTime;
                *existing = m;
                existing->memberId = id;
                existing->joinTime = joined;
            }
            else
            {
                MemberDoc added = m;
                added.memberId = stored.nextMemberId++;
                added.joinTime = winrt::clock::now();
                stored.members.push_back(added);
            }
        }

        stored.changeNumber++;
        stored.writeStatus = created ? mp::WriteSessionStatus::Created : mp::WriteSessionStatus::Updated;
        auto snapshot = std::make_shared<SessionDoc>(stored);

        if (stored.members.empty())
        {
            m_sessions.erase(written.Key());
            deleted = true;
        }
        return snapshot;
    }

    SessionPtr SessionStore::Get(const std::wstring& key)
    {
        std::lock_guard guard(m_lock);
        auto it = m_sessions.find(key);
        return it == m_sessions.end() ? nullptr : std::make_shared<SessionDoc>(it->second);
    }

    std::vector<SessionPtr> SessionStore::All()
    {
        std::lock_guard guard(m_lock);
        std::vector<SessionPtr> out;
        for (auto& [key, doc] : m_sessions) out.push_back(std::make_shared<SessionDoc>(doc));
        return out;
    }
}
