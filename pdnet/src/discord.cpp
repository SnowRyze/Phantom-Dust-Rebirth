#include "pch.h"
#include "discord.h"
#include <set>
#include "config.h"
#include "log.h"
#include <condition_variable>
#include <deque>
#include <thread>
#include <winhttp.h>
#include <winrt/Windows.Data.Json.h>
#pragma comment(lib, "winhttp.lib")

namespace pdnet::discord
{
    namespace
    {
        std::string JsonEscape(const std::string& s)
        {
            std::string out;
            for (unsigned char c : s)
            {
                if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
                else if (c < 0x20) { char buf[8]; sprintf_s(buf, "\\u%04x", c); out += buf; }
                else out += static_cast<char>(c);
            }
            return out;
        }

        // One Discord message per lobby: posted when the lobby opens, then edited in place when players join or leave
        // and when it closes. Its id is kept here and in the lobby itself (attribute PD_DISCORD), so if the host
        // leaves, the player who takes the lobby over keeps editing the same message.
        // Sending: POST <webhook>?wait=true (Discord answers with the message and its id), PATCH <webhook>/messages/<id>.
        struct Job
        {
            std::string lobbyId;
            std::string text;
            bool closing = false; // the lobby's last state: forget its message afterwards
            std::string closedText; // how the message reads once the lobby is gone
            bool alone = false;     // this PC is the only member: if the game closes now, the lobby ends with it
            bool opening = false;   // the "lobby opened" post: the only job that may create the message. Player posts
                                    // and "closed" only edit it, so a lobby its host keeps off Discord (lobby posts off)
                                    // gets no message at all, also from players who take the lobby over.
        };

        // Closing the game straight from the lobby (or a crash) gives pdnet no chance to edit the message, so while
        // this PC is alone in its lobby it keeps a note: the message id, then the "closed" text. pdlaunch, which
        // outlives the game, sends that edit after the game exits (tools\pdlaunch.cpp). With other players in the
        // lobby there is no note: one of them takes the lobby over and edits the message from then on.
        void ExitNote(const std::string& message, const std::string& closedText)
        {
            std::wstring dir = LocalStatePath() + L"\\pdnet", path = dir + L"\\discord-close.txt";
            if (message.empty())
            {
                DeleteFileW(path.c_str());
                return;
            }
            CreateDirectoryW(dir.c_str(), nullptr);
            if (FILE* f = nullptr; !_wfopen_s(&f, path.c_str(), L"wb") && f)
            {
                std::string note = message + "\n" + closedText;
                fwrite(note.data(), 1, note.size(), f);
                fclose(f);
            }
        }
        std::mutex g_queueLock;
        std::condition_variable g_queueReady;
        std::deque<Job> g_queue;
        bool g_threadStarted = false;
        std::map<std::string, std::string> g_messages; // lobby id -> Discord message id (under g_queueLock)

        // Sends one request; returns the HTTP status (0 if Discord wasn't reached) and the response body.
        DWORD Request(const std::wstring& url, const wchar_t* method, const std::wstring& suffix, const std::string& text, std::string* response)
        {
            URL_COMPONENTS parts{ sizeof(parts) };
            wchar_t host[256], path[2048], extra[1024];
            parts.lpszHostName = host;
            parts.dwHostNameLength = _countof(host);
            parts.lpszUrlPath = path;
            parts.dwUrlPathLength = _countof(path);
            parts.lpszExtraInfo = extra;
            parts.dwExtraInfoLength = _countof(extra);
            if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts))
            {
                Log(L"discord: discord_webhook is not a valid URL");
                return 0;
            }
            // suffix: "/messages/<id>" goes on the path, "?wait=true" joins any query the webhook URL already has.
            std::wstring target = path;
            std::wstring query = extra;
            if (!suffix.empty() && suffix[0] == L'?') query = query.empty() ? suffix : query + L"&" + suffix.substr(1);
            else target += suffix;
            target += query;
            std::string body = "{\"content\":\"" + JsonEscape(text) + "\",\"allowed_mentions\":{\"parse\":[]}}";
            DWORD status = 0;
            HINTERNET session = WinHttpOpen(L"pdnet/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
            HINTERNET connect = session ? WinHttpConnect(session, host, parts.nPort, 0) : nullptr;
            HINTERNET request = connect ? WinHttpOpenRequest(connect, method, target.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
            if (request &&
                WinHttpSendRequest(request, L"Content-Type: application/json\r\n", static_cast<DWORD>(-1), body.data(), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
                WinHttpReceiveResponse(request, nullptr))
            {
                DWORD size = sizeof(status);
                WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
                if (response)
                {
                    char buf[4096];
                    for (DWORD got = 0; WinHttpReadData(request, buf, sizeof(buf), &got) && got;) response->append(buf, got);
                }
                if (status < 200 || status >= 300) Log(L"discord: Discord answered HTTP %lu to %s", status, method);
            }
            else
            {
                Log(L"discord: could not reach Discord (error %lu)", GetLastError());
            }
            if (request) WinHttpCloseHandle(request);
            if (connect) WinHttpCloseHandle(connect);
            if (session) WinHttpCloseHandle(session);
            return status;
        }

        void Send(const std::wstring& url, const Job& job)
        {
            std::string message;
            {
                std::lock_guard guard(g_queueLock);
                auto it = g_messages.find(job.lobbyId);
                if (it != g_messages.end()) message = it->second;
            }
            DWORD status = 0;
            if (message.empty() && !job.opening)
            {
                Log(L"discord: lobby %S has no message (not announced) - nothing posted", job.lobbyId.c_str());
                return;
            }
            if (!message.empty())
            {
                status = Request(url, L"PATCH", L"/messages/" + std::wstring(winrt::to_hstring(message)), job.text, nullptr);
                if (status == 404) message.clear(); // someone deleted the message: post a new one (only when opening)
            }
            if (message.empty() && job.opening)
            {
                std::string response;
                status = Request(url, L"POST", L"?wait=true", job.text, &response);
                winrt::Windows::Data::Json::JsonObject posted{ nullptr };
                if (status >= 200 && status < 300 && winrt::Windows::Data::Json::JsonObject::TryParse(winrt::to_hstring(response), posted))
                    message = winrt::to_string(posted.GetNamedString(L"id", L""));
                if (!message.empty() && !job.closing)
                {
                    {
                        std::lock_guard guard(g_queueLock);
                        g_messages[job.lobbyId] = message;
                    }
                    // For whoever takes the lobby over (owner only; a non-owner's change is logged and dropped).
                    lobbies::Update(job.lobbyId, lobbies::Changes{ { { "PD_DISCORD", message } }, {} }, lobbies::Changes{});
                }
            }
            if (job.closing)
            {
                std::lock_guard guard(g_queueLock);
                g_messages.erase(job.lobbyId);
            }
            // No note while Discord lobby posts are off: "closed" is a lobby post (a message can still exist when only
            // player posts are on, since those edit the lobby's message).
            ExitNote(job.closing || !job.alone || !GetConfig().discordLobbyMessages ? std::string() : message, job.closedText);
        }

        // Queues the lobby's message as it should now read. A newer state replaces a queued one for the same lobby,
        // so a burst of joins becomes one edit.
        void Show(const std::string& lobbyId, const std::string& text, bool closing, const std::string& closedText = {}, bool alone = false, bool opening = false)
        {
            const std::wstring& url = GetConfig().discordWebhook;
            if (url.empty()) return;
            std::lock_guard guard(g_queueLock);
            auto queued = std::find_if(g_queue.begin(), g_queue.end(), [&](const Job& j) { return j.lobbyId == lobbyId; });
            if (queued != g_queue.end())
            {
                queued->text = text;
                queued->closing |= closing;
                queued->closedText = closedText;
                queued->alone = alone;
                queued->opening |= opening;
            }
            else g_queue.push_back(Job{ lobbyId, text, closing, closedText, alone, opening });
            if (!g_threadStarted)
            {
                g_threadStarted = true;
                std::thread([url]
                {
                    for (;;)
                    {
                        Job next;
                        {
                            std::unique_lock lock(g_queueLock);
                            g_queueReady.wait(lock, [] { return !g_queue.empty(); });
                            next = std::move(g_queue.front());
                            g_queue.pop_front();
                        }
                        Send(url, next);
                    }
                }).detach();
            }
            g_queueReady.notify_one();
        }

        struct Announced
        {
            std::string name;
            uint32_t members = 0;
            bool opened = false; // "opened" waits for the game's search handle, which says the game type and rule
            std::map<std::string, std::string> players; // EOS product user id -> player name
            std::string match;       // "Tag Team · Standard Battle"
            uint32_t maxMembers = 0;
            // The latest join/leave, by user id. A member's PD_NAME arrives in a lobby update after the one that adds
            // them, so names are looked up when the message is drawn (`known` keeps the names of those who left).
            std::vector<std::string> joined, left;
            std::map<std::string, std::string> known;
        };
        std::mutex g_lock;
        std::map<std::string, Announced> g_lobbies; // lobbies this PC owns and announces
        std::set<std::string> g_gone;               // lobbies whose game host left: their message says closed (LobbyGone)

        bool Public(const lobbies::Lobby& l)
        {
            constexpr std::string_view kTemplate = "|SearchableGame";
            return l.bucket.size() >= kTemplate.size() && l.bucket.compare(l.bucket.size() - kTemplate.size(), kTemplate.size(), kTemplate) == 0;
        }

        uint32_t Members(const lobbies::Lobby& l) { return l.joined ? static_cast<uint32_t>(l.members.size()) : l.memberCount; }

        std::string HostName(const lobbies::Lobby& l)
        {
            std::string name = LobbyProperty(l, L"gamename");
            return name.empty() ? winrt::to_string(GetConfig().displayName) : name;
        }

        // Who is in a lobby this PC is a member of (each member's PD_NAME attribute is their display name).
        std::map<std::string, std::string> Roster(const lobbies::Lobby& l)
        {
            std::map<std::string, std::string> out;
            for (auto& m : l.members)
            {
                auto it = m.attributes.find("PD_NAME");
                out[m.userId] = it != m.attributes.end() ? it->second : std::string{};
            }
            return out;
        }

        // "**A**" or "**A**, **B**".
        std::string Names(const std::vector<std::string>& names)
        {
            std::string out;
            for (auto& n : names) out += (out.empty() ? "" : ", ") + ("**" + (n.empty() ? std::string("A player") : Name(n)) + "**");
            return out;
        }

        std::string Players(uint32_t members, uint32_t max)
        {
            return std::to_string(members) + "/" + std::to_string(max) + " players";
        }

        // The lobby's message as it should read now:
        //   🟢 **Host** opened a Phantom Dust lobby · Tag Team · Standard Battle · 2/4 players   (🎮 ... · full)
        //   Players: **Host**, **B**
        //   👥 **B** joined
        // and once closed: 🔴 **Host**'s lobby closed · Tag Team · Standard Battle
        std::string Card(const Announced& a, bool closed = false)
        {
            std::string match = a.match.empty() ? "" : " \xC2\xB7 " + a.match;
            if (closed) return "\xF0\x9F\x94\xB4 **" + Name(a.name) + "**'s lobby closed" + match;
            bool full = a.maxMembers && a.members >= a.maxMembers;
            auto names = [&](const std::vector<std::string>& ids)
            {
                std::vector<std::string> out;
                for (auto& id : ids)
                {
                    auto it = a.known.find(id);
                    out.push_back(it != a.known.end() ? it->second : std::string());
                }
                return out;
            };
            std::vector<std::string> everyone;
            for (auto& [id, name] : a.players) everyone.push_back(id);
            std::string text = (full ? "\xF0\x9F\x8E\xAE **" : "\xF0\x9F\x9F\xA2 **") + Name(a.name) + "** opened a Phantom Dust lobby" + match +
                               " \xC2\xB7 " + Players(a.members, a.maxMembers) + (full ? " \xC2\xB7 full" : "");
            if (!everyone.empty()) text += "\nPlayers: " + Names(names(everyone));
            std::string change;
            if (!a.joined.empty()) change = "\xF0\x9F\x91\xA5 " + Names(names(a.joined)) + " joined";
            if (!a.left.empty()) change += (change.empty() ? "" : "  ") + std::string("\xF0\x9F\x91\x8B ") + Names(names(a.left)) + " left";
            if (!change.empty()) text += "\n" + change;
            return text;
        }

        // Fills in the lobby once it carries the game's search handle (game type and rule). Called with g_lock held;
        // returns true when the message should go out now.
        bool Open(const lobbies::Lobby& l, Announced& a)
        {
            if (a.opened || lobbies::Joined(l.attributes, "PD_SEARCH").empty()) return false;
            a.opened = true;
            a.members = Members(l);
            a.players = Roster(l);
            for (auto& [id, name] : a.players) if (!name.empty()) a.known[id] = name;
            a.match = MatchDescription(l);
            a.maxMembers = l.maxMembers;
            return GetConfig().discordLobbyMessages;
        }
    }

    bool PostNow(const std::wstring& url, const std::string& text)
    {
        DWORD status = Request(url, L"POST", L"", text, nullptr);
        return status >= 200 && status < 300;
    }

    std::string Name(const std::string& s)
    {
        std::string out;
        for (char c : s)
        {
            if (strchr("\\*_~`|>#", c)) out += '\\';
            out += c;
        }
        return out.empty() ? "Someone" : out;
    }

    std::string LobbyProperty(const lobbies::Lobby& l, const wchar_t* name)
    {
        winrt::Windows::Data::Json::JsonObject props{ nullptr };
        std::string json = lobbies::Joined(l.attributes, "PD_PROPS");
        if (json.empty() || !winrt::Windows::Data::Json::JsonObject::TryParse(winrt::to_hstring(json), props) || !props.HasKey(name)) return {};
        auto v = props.Lookup(name);
        std::string text = v.ValueType() == winrt::Windows::Data::Json::JsonValueType::String ? winrt::to_string(v.GetString()) : winrt::to_string(v.Stringify());
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"') text = text.substr(1, text.size() - 2);
        return text;
    }

    namespace
    {
        // A number the game put in its search handle (PD_SEARCH: {"numbers": {...}, "strings": {...}}), or -1.
        int SearchNumber(const lobbies::Lobby& l, const wchar_t* name)
        {
            winrt::Windows::Data::Json::JsonObject search{ nullptr };
            std::string json = lobbies::Joined(l.attributes, "PD_SEARCH");
            if (json.empty() || !winrt::Windows::Data::Json::JsonObject::TryParse(winrt::to_hstring(json), search)) return -1;
            auto numbers = search.GetNamedObject(L"numbers", winrt::Windows::Data::Json::JsonObject{});
            return numbers.HasKey(name) ? static_cast<int>(numbers.GetNamedNumber(name)) : -1;
        }
    }

    // The game's own names (Assets\Data\Menu\livesession_en.ssb lists them in this order).
    std::string MatchDescription(const lobbies::Lobby& l)
    {
        static const char* kTypes[] = { "Battle Royale", "Tag Team", "One-on-One" };
        static const char* kRules[] = { "Standard Battle", "Ante", "Quick", "Puny", "Heavy", "Special Environment",
                                        "Single School", "Highlander", "Puny Only", "Heavy Only", "KO Battle", "Annihilate the Odd One" };
        int type = SearchNumber(l, L"gametype"), rule = SearchNumber(l, L"gamerule");
        std::string text;
        if (type >= 0 && type < static_cast<int>(std::size(kTypes))) text = kTypes[type];
        if (rule >= 0 && rule < static_cast<int>(std::size(kRules))) text += (text.empty() ? "" : " \xC2\xB7 ") + std::string(kRules[rule]);
        // The lobby's Rating option (rated_lobbies): PD_RATED (kept through matches, see xsapi/mpsd.h), else the
        // search handle's ratinglimit.
        if (GetConfig().ratedLobbies)
        {
            auto stored = l.attributes.find("PD_RATED");
            int rated = stored != l.attributes.end() && !stored->second.empty() ? atoi(stored->second.c_str()) : SearchNumber(l, L"ratinglimit");
            text += (text.empty() ? "" : " \xC2\xB7 ") + std::string(rated > 0 ? "Rated" : "Unrated"); // no setting at all: unrated
        }
        return text;
    }

    void LobbyOpened(const lobbies::Lobby& l)
    {
        if (GetConfig().discordWebhook.empty() || !Public(l)) return;
        std::string text, closed;
        bool alone = false;
        {
            std::lock_guard guard(g_lock);
            if (g_lobbies.count(l.id)) return;
            Announced& a = g_lobbies[l.id] = Announced{ HostName(l), Members(l) };
            if (Open(l, a))
            {
                text = Card(a);
                closed = Card(a, true);
                alone = a.members <= 1;
            }
        }
        if (!text.empty()) Show(l.id, text, false, closed, alone, true);
    }

    void LobbyChanged(const lobbies::Lobby& l)
    {
        if (GetConfig().discordWebhook.empty() || !Public(l)) return;
        bool mine = l.joined && l.ownerId == lobbies::LocalUserId();
        uint32_t members = Members(l);
        std::string text, closed;
        bool alone = false, opening = false;
        {
            std::lock_guard guard(g_lock);
            if (g_gone.count(l.id)) return; // already shown as closed (LobbyGone)
            auto it = g_lobbies.find(l.id);
            if (!mine)
            {
                if (it != g_lobbies.end()) g_lobbies.erase(it); // the new owner announces from now on
                return;
            }
            if (it == g_lobbies.end())
            {
                // Handed to this PC (host migration): keep editing the message the old host posted.
                Announced a{ HostName(l), members, true, Roster(l), MatchDescription(l), l.maxMembers };
                g_lobbies[l.id] = a;
                auto posted = l.attributes.find("PD_DISCORD");
                if (posted != l.attributes.end() && !posted->second.empty())
                {
                    std::lock_guard queue(g_queueLock);
                    g_messages[l.id] = posted->second;
                }
                return;
            }
            Announced& a = it->second;
            if (!a.opened)
            {
                if (Open(l, a)) text = Card(a);
                opening = !text.empty();
            }
            else if (members > 0)
            {
                auto now = Roster(l);
                bool named = false; // a member's name arrived since the message was last drawn
                for (auto& [id, name] : now)
                    if (!name.empty() && a.known[id] != name)
                    {
                        a.known[id] = name;
                        named = true;
                    }
                if (a.members != members)
                {
                    a.joined.clear();
                    a.left.clear();
                    for (auto& [id, name] : now) if (!a.players.count(id)) a.joined.push_back(id);
                    for (auto& [id, name] : a.players) if (!now.count(id)) a.left.push_back(id);
                    a.members = members;
                    a.players = std::move(now);
                    a.maxMembers = l.maxMembers;
                }
                else if (!named) return;
                if (GetConfig().discordPlayerMessages) text = Card(a);
            }
            if (!text.empty())
            {
                closed = Card(a, true);
                alone = a.members <= 1;
            }
        }
        if (!text.empty()) Show(l.id, text, false, closed, alone, opening);
    }

    void LobbyLeft(const std::string& id, bool lastMember)
    {
        std::string text;
        {
            std::lock_guard guard(g_lock);
            auto it = g_lobbies.find(id);
            if (it == g_lobbies.end()) return;
            if (lastMember && it->second.opened && GetConfig().discordLobbyMessages) text = Card(it->second, true);
            g_lobbies.erase(it);
        }
        // Leaving with others still in: EOS hands the lobby to one of them, whose game edits the message from now on.
        if (!text.empty()) Show(id, text, true);
        else if (!lastMember)
        {
            {
                std::lock_guard queue(g_queueLock);
                g_messages.erase(id);
            }
            ExitNote({}, {}); // the lobby goes on without this PC
        }
    }

    void LobbyLost(const std::string& id) { LobbyLeft(id, true); }

    void LobbyGone(const lobbies::Lobby& l)
    {
        if (GetConfig().discordWebhook.empty() || !Public(l)) return;
        std::string text;
        {
            std::lock_guard guard(g_lock);
            if (!g_gone.insert(l.id).second) return;
            auto it = g_lobbies.find(l.id);
            Announced a = it != g_lobbies.end() ? it->second : Announced{ HostName(l), Members(l), true, Roster(l), MatchDescription(l), l.maxMembers };
            if (it != g_lobbies.end()) g_lobbies.erase(it);
            if (!a.opened || !GetConfig().discordLobbyMessages) return;
            text = Card(a, true);
        }
        // The message may have been posted by the old host: its id is in the lobby (PD_DISCORD).
        auto posted = l.attributes.find("PD_DISCORD");
        if (posted != l.attributes.end() && !posted->second.empty())
        {
            std::lock_guard queue(g_queueLock);
            if (!g_messages.count(l.id)) g_messages[l.id] = posted->second;
        }
        Show(l.id, text, true);
    }
}
