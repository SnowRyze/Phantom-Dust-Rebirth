#pragma once
// pdnet's model of a multiplayer session (what Xbox Live's session directory, MPSD, keeps per session).
// The XSAPI-facing classes (MultiplayerSession, its members/properties/constants) are views onto a SessionDoc.
// Stage A: SessionStore keeps documents in memory on this PC. Stage B will back it with EOS lobbies.
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pdnet::mpsd
{
    namespace mp = winrt::Microsoft::Xbox::Services::Multiplayer;

    struct MemberDoc
    {
        uint32_t memberId = 0;
        std::wstring xuid;
        std::wstring gamertag;
        std::wstring deviceAddressBase64;
        std::wstring customConstantsJson;
        std::map<std::wstring, std::wstring> customProperties; // name -> JSON value
        mp::MultiplayerSessionMemberStatus status = mp::MultiplayerSessionMemberStatus::Active;
        bool initializeRequested = false;
        winrt::Windows::Foundation::DateTime joinTime{};
        bool leaving = false; // set by Leave(); removed from the stored session on write
    };

    struct SessionDoc
    {
        std::wstring scid, templateName, sessionName;
        uint32_t maxMembers = 4;
        mp::MultiplayerSessionVisibility visibility = mp::MultiplayerSessionVisibility::Open;
        std::vector<std::wstring> initiators;
        std::wstring customConstantsJson;
        std::map<std::wstring, std::wstring> customProperties; // name -> JSON value
        std::vector<std::wstring> keywords;
        mp::MultiplayerSessionRestriction joinRestriction = mp::MultiplayerSessionRestriction::None;
        mp::MultiplayerSessionRestriction readRestriction = mp::MultiplayerSessionRestriction::None;
        std::wstring hostDeviceToken;
        bool closed = false;
        std::vector<MemberDoc> members;
        uint32_t nextMemberId = 1;
        uint64_t changeNumber = 0;
        std::wstring correlationId;
        mp::MultiplayerSessionChangeTypes subscribedChanges = mp::MultiplayerSessionChangeTypes::None;
        mp::WriteSessionStatus writeStatus = mp::WriteSessionStatus::Unknown;

        std::wstring Key() const { return scid + L"|" + templateName + L"|" + sessionName; }
        std::wstring SessionPropertiesJson() const; // {"name":value,...}
        MemberDoc* FindMember(const std::wstring& xuid);
    };

    using SessionPtr = std::shared_ptr<SessionDoc>;

    // Joins a JSON object's members: {"a":1,"b":"x"}
    std::wstring JsonObject(const std::map<std::wstring, std::wstring>& values);

    class SessionStore
    {
    public:
        static SessionStore& Instance();

        // Merges what this PC wrote (session-level fields, plus its own member entry) into the stored copy and returns
        // a fresh snapshot. `created` reports a new session. Returns null if the write mode can't be satisfied.
        SessionPtr Write(const SessionDoc& written, mp::MultiplayerSessionWriteMode mode, bool& created, bool& deleted);

        SessionPtr Get(const std::wstring& key);
        std::vector<SessionPtr> All();

    private:
        std::mutex m_lock;
        std::map<std::wstring, SessionDoc> m_sessions;
    };
}
