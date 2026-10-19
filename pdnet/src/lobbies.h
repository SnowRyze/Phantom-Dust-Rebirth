#pragma once
// EOS lobbies for stage B (multiplayer sessions shared between PCs; see xsapi\mpsd.h for how sessions map onto lobbies).
// Plain C++ so the XSAPI stand-in (compiled without the EOS headers) can use it. Every call is bridged onto the EOS
// thread (eos::Post) and waits for its EOS callback on the calling thread, with a timeout, so a lost callback never
// hangs the game. Don't call these from the EOS thread itself.
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pdnet::lobbies
{
    using Attributes = std::map<std::string, std::string>; // UTF-8 key -> UTF-8 value (all values are strings)

    struct Member
    {
        std::string userId; // EOS product user id
        Attributes attributes;
    };

    struct Lobby
    {
        std::string id;
        std::string ownerId;
        std::string bucket;
        uint32_t maxMembers = 0;
        bool joined = false; // this PC is a member (details are live) rather than a search result
        Attributes attributes;
        std::vector<Member> members;
    };

    struct Changes
    {
        Attributes set;
        std::vector<std::string> remove;
        bool empty() const { return set.empty() && remove.empty(); }
    };

    enum class Event { LobbyUpdated, MemberUpdated, MemberJoined, MemberLeft, Promoted, Closed, Kicked };

    // True once EOS is signed in (lobby calls fail fast before that).
    bool Available();
    std::string LocalUserId();

    // Creates a public lobby with a fixed id (join by id enabled, host migration on, no presence) and sets the first
    // lobby and member attributes. Returns the lobby as EOS now reports it.
    std::optional<Lobby> Create(const std::string& id, const std::string& bucket, uint32_t maxMembers, const Attributes& lobbyAttributes, const Attributes& memberAttributes);
    std::optional<Lobby> Join(const std::string& id, const Attributes& memberAttributes);

    // One modification: lobby-level changes are applied only if this PC owns the lobby (logged otherwise).
    // maxMembers 0 = unchanged.
    bool Update(const std::string& id, const Changes& lobby, const Changes& member, uint32_t maxMembers = 0);

    bool Leave(const std::string& id);
    bool Destroy(const std::string& id);
    bool Promote(const std::string& id, const std::string& userId);

    // A lobby this PC is in (live copy), else a one-lobby search by id. Empty if it doesn't exist.
    std::optional<Lobby> Read(const std::string& id);

    // Public lobbies in `bucket` (up to 200).
    std::vector<Lobby> Search(const std::string& bucket, uint32_t maxResults = 100);

    // Called on a worker thread (never the EOS thread) for every lobby change notification EOS delivers.
    void SetListener(std::function<void(const std::string& lobbyId, Event event, const std::string& userId)> listener);

    // Called on a worker thread when the EOS sign-in is lost (every lobby membership is gone with it).
    void SetSignOutListener(std::function<void()> listener);

    // Long values don't fit in one EOS attribute: Split stores `value` as key, key.1, key.2, ... (and key# = count
    // when there's more than one piece); Joined reads it back. RemoveSplit lists the keys an old value used.
    void Split(Attributes& out, const std::string& key, const std::string& value);
    std::string Joined(const Attributes& in, const std::string& key);
    std::vector<std::string> SplitKeys(const Attributes& in, const std::string& key);
}
