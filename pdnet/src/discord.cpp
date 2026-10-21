#include "pch.h"
#include "discord.h"
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

        // Announcements go out one at a time, in order, on their own thread.
        std::mutex g_queueLock;
        std::condition_variable g_queueReady;
        std::deque<std::string> g_queue;
        bool g_threadStarted = false;

        void Post(const std::string& text)
        {
            const std::wstring& url = GetConfig().discordWebhook;
            if (url.empty()) return;
            std::lock_guard guard(g_queueLock);
            g_queue.push_back(text);
            if (!g_threadStarted)
            {
                g_threadStarted = true;
                std::thread([url]
                {
                    for (;;)
                    {
                        std::string next;
                        {
                            std::unique_lock lock(g_queueLock);
                            g_queueReady.wait(lock, [] { return !g_queue.empty(); });
                            next = std::move(g_queue.front());
                            g_queue.pop_front();
                        }
                        PostNow(url, next);
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
        };
        std::mutex g_lock;
        std::map<std::string, Announced> g_lobbies; // lobbies this PC owns and announces

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

        std::string Players(uint32_t members, uint32_t max)
        {
            return std::to_string(members) + "/" + std::to_string(max) + " players";
        }

        // Posts "opened" once the lobby carries the game's search handle. Called with g_lock held.
        std::string OpenedText(const lobbies::Lobby& l, Announced& a)
        {
            if (a.opened || lobbies::Joined(l.attributes, "PD_SEARCH").empty()) return {};
            a.opened = true;
            a.members = Members(l);
            std::string match = MatchDescription(l);
            return "\xF0\x9F\x9F\xA2 **" + Name(a.name) + "** opened a Phantom Dust lobby \xC2\xB7 " + (match.empty() ? "" : match + " \xC2\xB7 ") +
                   Players(a.members, l.maxMembers);
        }
    }

    bool PostNow(const std::wstring& url, const std::string& text)
    {
        URL_COMPONENTS parts{ sizeof(parts) };
        wchar_t host[256], path[2048];
        parts.lpszHostName = host;
        parts.dwHostNameLength = _countof(host);
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = _countof(path);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts))
        {
            Log(L"discord: discord_webhook is not a valid URL");
            return false;
        }
        std::string body = "{\"content\":\"" + JsonEscape(text) + "\",\"allowed_mentions\":{\"parse\":[]}}";
        bool ok = false;
        HINTERNET session = WinHttpOpen(L"pdnet/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
        HINTERNET connect = session ? WinHttpConnect(session, host, parts.nPort, 0) : nullptr;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", path, nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
        if (request &&
            WinHttpSendRequest(request, L"Content-Type: application/json\r\n", static_cast<DWORD>(-1), body.data(), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
            WinHttpReceiveResponse(request, nullptr))
        {
            DWORD status = 0, size = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
            ok = status >= 200 && status < 300;
            if (!ok) Log(L"discord: Discord answered HTTP %lu", status);
        }
        else
        {
            Log(L"discord: could not reach Discord (error %lu)", GetLastError());
        }
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        return ok;
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
        return text;
    }

    void LobbyOpened(const lobbies::Lobby& l)
    {
        if (GetConfig().discordWebhook.empty() || !Public(l)) return;
        std::string text;
        {
            std::lock_guard guard(g_lock);
            if (g_lobbies.count(l.id)) return;
            text = OpenedText(l, g_lobbies[l.id] = Announced{ HostName(l), Members(l) });
        }
        if (!text.empty()) Post(text);
    }

    void LobbyChanged(const lobbies::Lobby& l)
    {
        if (GetConfig().discordWebhook.empty() || !Public(l)) return;
        bool mine = l.joined && l.ownerId == lobbies::LocalUserId();
        uint32_t members = Members(l);
        std::string text;
        {
            std::lock_guard guard(g_lock);
            auto it = g_lobbies.find(l.id);
            if (!mine)
            {
                if (it != g_lobbies.end()) g_lobbies.erase(it); // the new owner announces from now on
                return;
            }
            if (it == g_lobbies.end())
            {
                g_lobbies[l.id] = Announced{ HostName(l), members, true }; // handed to this PC (host migration): announce changes from now on
                return;
            }
            Announced& a = it->second;
            if (!a.opened)
            {
                text = OpenedText(l, a);
            }
            else if (a.members != members && members > 0)
            {
                a.members = members;
                text = members >= l.maxMembers ? "\xF0\x9F\x8E\xAE **" + Name(a.name) + "**'s lobby is full \xC2\xB7 " + Players(members, l.maxMembers)
                                               : "\xF0\x9F\x91\xA5 **" + Name(a.name) + "**'s lobby \xC2\xB7 " + Players(members, l.maxMembers);
            }
        }
        if (!text.empty()) Post(text);
    }

    void LobbyLeft(const std::string& id, bool lastMember)
    {
        std::string name;
        bool opened = false;
        {
            std::lock_guard guard(g_lock);
            auto it = g_lobbies.find(id);
            if (it == g_lobbies.end()) return;
            name = it->second.name;
            opened = it->second.opened;
            g_lobbies.erase(it);
        }
        // Leaving with others still in: EOS hands the lobby to one of them, whose game announces it from now on.
        if (lastMember && opened) Post("\xF0\x9F\x94\xB4 **" + Name(name) + "**'s lobby closed");
    }

    void LobbyLost(const std::string& id) { LobbyLeft(id, true); }
}
