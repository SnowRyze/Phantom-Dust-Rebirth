#pragma once
// pdnet's model of a multiplayer session (what Xbox Live's session directory, MPSD, keeps per session).
// The XSAPI-facing classes (MultiplayerSession, its members/properties/constants) are views onto a SessionDoc.
//
// Stage A kept documents in memory on one PC. Stage B (eos=on and EOS signed in) backs each session with an EOS lobby,
// so other PCs can find, read and join it. Without EOS the store behaves exactly as in stage A.
//
// What the game does (stage A log, 2026-10-19): creates SearchableGame/<guid> (max 4) with SessionStore::Write,
// joins, writes it ~6 times (custom properties, host device token, member address), then SetSearchHandleAsync with
// NumbersMetadata/StringsMetadata. Its lobby browser (PDUWP.exe strings) calls GetSearchHandlesAsync with filters
// such as "numbers/ratingmin le N and numbers/ratingmax ge N", "numbers/reqmoney le N", "numbers/cross eq 0" and
// "numbers/ratinglimit eq N"; session properties it uses include state, stage, sessiontype, crossplay and rating.
//
// Session <-> lobby mapping. Every attribute is a public string attribute; values longer than 500 bytes are split
// (lobbies::Split: KEY, KEY.1, ..., KEY# = piece count).
//
//   lobby id        "pd1" + 40 hex digits of SHA-256(Key()). Fixed, so any PC turns a session reference (or a search
//                   handle, whose id is the lobby id) into the lobby without a lookup service; join-by-id is enabled.
//   bucket          "pd<version>|<scid>|<template>". Searches only ever look inside one bucket, so mismatched mod
//                   versions never see each other (PD_VERSION below is informational).
//   max members     lobby max members (owner only)
//   owner           the EOS lobby owner. Only the owner can change lobby attributes, so the owner writes every
//                   session-level field. A non-owner's session-level edits go to its PD_REQ member attribute and the
//                   owner applies them (any member can write a session in MPSD). When the host device token names
//                   another member, the owner promotes that member, so the game's host and the lobby owner stay the
//                   same PC (EOS host migration covers an owner who drops).
//
//   Lobby attribute  SessionDoc field                       Member attribute  MemberDoc field
//   PD_VERSION       (mod version, "1")                     PD_XUID           xuid (pdnet's stable player id, LocalXuid)
//   PD_SCID          scid                                   PD_NAME           gamertag (display_name)
//   PD_TEMPLATE      templateName                           PD_ADDR           deviceAddressBase64 (34-byte pdnet snapshot)
//   PD_NAME          sessionName                            PD_MCONST         customConstantsJson
//   PD_CORR          correlationId (also the Branch)        PD_MPROPS         customProperties (JSON object name -> raw JSON text)
//   PD_CONST         customConstantsJson (set at create)    PD_STATUS         status (enum value)
//   PD_PROPS         customProperties (as PD_MPROPS)        PD_INIT           initializeRequested
//   PD_KEYWORDS      keywords (JSON array)                  PD_JOIN           joinTime (DateTime ticks)
//   PD_INITIATORS    initiators (JSON array)                PD_REQ            session-level edits for the owner to apply
//   PD_VIS/PD_JOINR/PD_READR  visibility, join/read restriction (enum values)
//   PD_CLOSED        closed ("0"/"1")
//   PD_HOST          hostDeviceToken (= the host's xuid, since DeviceToken() is the xuid)
//   PD_SEARCH        search handle (JSON: tags, numbers, strings, created)
//   PD_MIDS          xuid -> memberId (JSON object), assigned by the owner in join order so every PC numbers members
//                    the same way; PD_NEXTMID is the next id. A member not numbered yet gets a provisional id locally.
//
//   changeNumber, subscribedChanges and writeStatus are local to each PC: changeNumber goes up on every write and on
//   every lobby change EOS reports, which also raises MultiplayerService::MultiplayerSessionChanged.
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <functional>

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
        bool leaving = false;     // set by Leave(); removed from the stored session on write
        bool reservation = false; // added by AddMemberReservation (kept on this PC only)
    };

    struct SearchHandle
    {
        std::wstring id; // = the lobby id
        std::vector<std::wstring> tags;
        std::map<std::wstring, double> numbers;
        std::map<std::wstring, std::wstring> strings;
        winrt::Windows::Foundation::DateTime created{};
    };

    // Which session-level fields the game changed on this copy; a write applies only these.
    struct Edits
    {
        bool maxMembers = false, visibility = false, joinRestriction = false, readRestriction = false;
        bool host = false, closed = false, keywords = false;
        std::set<std::wstring> properties; // set or deleted
        bool any() const { return maxMembers || visibility || joinRestriction || readRestriction || host || closed || keywords || !properties.empty(); }
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
        std::optional<SearchHandle> search;
        std::wstring ownerXuid; // xuid of the lobby owner (empty for a local-only session)
        bool shared = false;    // backed by an EOS lobby
        uint32_t lobbyMemberCount = 0; // member count EOS reports (a search result has the count but not the members)
        Edits edits;

        std::wstring Key() const { return scid + L"|" + templateName + L"|" + sessionName; }
        std::wstring SessionPropertiesJson() const; // {"name":value,...}
        MemberDoc* FindMember(const std::wstring& xuid);
        const MemberDoc* FindMember(const std::wstring& xuid) const;
    };

    using SessionPtr = std::shared_ptr<SessionDoc>;

    // Joins a JSON object's members: {"a":1,"b":"x"}
    std::wstring JsonObject(const std::map<std::wstring, std::wstring>& values);

    // What changed from `before` to `after`, as XSAPI reports it in session change events.
    mp::MultiplayerSessionChangeTypes Compare(const SessionDoc& after, const SessionDoc& before);

    // The game's search filter ("numbers/x le 3 and strings/y eq 'z'"), evaluated against a session's search handle.
    bool MatchesFilter(const SearchHandle& handle, const std::wstring& filter);

    std::wstring LobbyIdFor(const std::wstring& key);

    class SessionStore
    {
    public:
        static SessionStore& Instance();

        // Merges what this PC wrote (session-level edits, plus its own member entry) into the stored copy and returns
        // a fresh snapshot. `created` reports a new session. Returns null if the write mode can't be satisfied
        // (or a join was refused); `status` then says why.
        SessionPtr Write(const SessionDoc& written, mp::MultiplayerSessionWriteMode mode, bool& created, bool& deleted, mp::WriteSessionStatus& status);

        // The stored copy, else (with EOS) the lobby other PCs created. `handleOrLobbyId` looks up by lobby id instead.
        SessionPtr Get(const std::wstring& key);
        SessionPtr GetByHandle(const std::wstring& handleOrLobbyId);
        std::vector<SessionPtr> All();

        // Search handles: stored with the session (and its lobby), found by SearchHandles.
        bool SetSearchHandle(const std::wstring& key, SearchHandle handle);
        int SetHostedMaxMembers(uint32_t maxMembers); // shared sessions whose lobby this PC owns
        bool ClearSearchHandle(const std::wstring& handleId);
        std::vector<SessionPtr> SearchHandles(const std::wstring& scid, const std::wstring& templateName);

        // Raised (on a worker thread, no lock held) when another PC changes a session this PC is in.
        using ChangedHandler = std::function<void(SessionPtr session, mp::MultiplayerSessionChangeTypes changes)>;
        void SetHandlers(ChangedHandler changed, std::function<void()> subscriptionLost);

    private:
        SessionStore();
        using Raised = std::vector<std::pair<SessionPtr, mp::MultiplayerSessionChangeTypes>>;
        bool Push(SessionDoc& doc);   // sends this PC's changes to the lobby it is in, then reloads the doc from it
        bool OwnerDuties(const std::string& lobbyId);
        void OnLobbyEvent(const std::string& lobbyId, int event, const std::string& userId);
        void OnSignedOut();
        void Raise(const Raised& raised);

        std::mutex m_writeLock; // serializes writes and lobby updates (held while waiting on EOS)
        std::mutex m_lock;      // guards m_sessions and the handlers (never held while waiting on EOS)
        std::map<std::wstring, SessionDoc> m_sessions;
        std::map<std::string, uint64_t> m_requestsApplied; // "lobby|user" -> last PD_REQ sequence applied (as owner)
        std::map<std::wstring, std::string> m_myRequests;  // key -> this PC's pending PD_REQ (as non-owner)
        ChangedHandler m_changed;
        std::function<void()> m_subscriptionLost;
    };
}
