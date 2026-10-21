// lobbytool: a second EOS client on the same PC, for testing stage B without a second machine.
// It is built from pdnet's own lobbies.cpp / eos.cpp, but runs as a normal desktop program, so the EOS SDK keeps its
// device ID in Windows Credential Manager instead of the game's LocalState: it signs in as a different product user.
//
//   lobbytool search [template]          list the game's lobbies (bucket pd1|<scid>|<template>, default SearchableGame)
//   lobbytool show <lobby id>            one lobby with all attributes and members
//   lobbytool join <lobby id> [seconds]  join as a fake player "lobbytool", print change notifications, then leave
//   lobbytool host <lobby id> [seconds]  self-test of the lobby layer in a separate bucket (create, search, update, destroy)
//   lobbytool p2ppeer [seconds] [force]   fake remote player over EOS P2P against the game's lobby (network_mode=eos):
//                                        ping, connect, HELLO, heartbeats, delete. force = Epic relays only.
//
// Needs EOSSDK-Win64-Shipping.dll and eos.ini next to lobbytool.exe (build-lobbytool.cmd copies them).
#include "pch.h"
#include "config.h"
#include "eos.h"
#include "lobbies.h"
#include "discord.h"
#include "net.h"
#include "p2p.h"
#include "log.h"
#include <cstdarg>
#include <cstdio>
#include <thread>
#include <set>

namespace pdnet
{
    // pdnet support functions, desktop versions.
    void Log(const wchar_t* fmt, ...)
    {
        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t line[4096];
        va_list args;
        va_start(args, fmt);
        _vsnwprintf_s(line, _TRUNCATE, fmt, args);
        va_end(args);
        fwprintf(stdout, L"%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line);
        fflush(stdout);
    }

    const std::wstring& LocalStatePath()
    {
        static std::wstring path = []
        {
            wchar_t exe[MAX_PATH];
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            std::wstring dir = exe;
            dir = dir.substr(0, dir.find_last_of(L'\\')) + L"\\state";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir;
        }();
        return path;
    }

    Config g_config = []
    {
        Config c;
        c.eos = true;
        c.displayName = L"lobbytool";
        // p2ppeer: behave like a remote pdnet PC on EOS P2P, with its "game" socket on 43223 (not the real game's port).
        c.p2p = true;
        c.gamePort = 43223;
        c.controlPort = 43222;
        return c;
    }();

    const Config& GetConfig() { return g_config; }

    std::vector<uint8_t> PublishedPayload() { return {}; }

    // Desktop programs may use Credential Manager, so nothing is redirected (and the device ID differs from the game's).
    int RedirectCredentialStore(HMODULE, const std::wstring&) { return 0; }
}

namespace
{
    using namespace pdnet;
    const char* kScid = "164d0100-a5f0-44b8-90ce-bf3a0fede427"; // xboxservices.config PrimaryServiceConfigId

    void Print(const lobbies::Lobby& l)
    {
        wprintf(L"lobby %S  owner %S  bucket %S  members %u/%u%s\n", l.id.c_str(), l.ownerId.c_str(), l.bucket.c_str(), l.memberCount, l.maxMembers,
            l.joined ? L"  (joined)" : L"  (search result: member details hidden)");
        for (auto& [k, v] : l.attributes) wprintf(L"    %-14S %S\n", k.c_str(), v.c_str());
        for (auto& m : l.members)
        {
            wprintf(L"  member %S%s\n", m.userId.c_str(), m.userId == l.ownerId ? L" (owner)" : L"");
            for (auto& [k, v] : m.attributes) wprintf(L"      %-12S %S\n", k.c_str(), v.c_str());
        }
    }

    // ---- p2ppeer: a fake remote player reached over EOS P2P (same game protocol as src\testpeer.cpp) ----
    std::atomic<int> g_hellos{ 0 }, g_heartbeats{ 0 }, g_other{ 0 };
    std::atomic<uint16_t> g_seq{ 1 };
    std::atomic<bool> g_stop{ false };

    std::vector<uint8_t> GameMessage(uint8_t type, uint16_t flags, const std::vector<uint8_t>& body, uint16_t seq = 0)
    {
        if (!seq) seq = g_seq++;
        uint16_t length = static_cast<uint16_t>(7 + body.size());
        std::vector<uint8_t> m(7);
        memcpy(&m[0], &seq, 2);
        m[2] = type;
        memcpy(&m[3], &length, 2);
        memcpy(&m[5], &flags, 2);
        m.insert(m.end(), body.begin(), body.end());
        return m;
    }

    std::vector<uint8_t> Hello(bool responding)
    {
        std::wstring name = L"lobbytool-p2p";
        uint16_t chars = static_cast<uint16_t>(name.size());
        std::vector<uint8_t> body = { static_cast<uint8_t>(responding), static_cast<uint8_t>(chars & 0xff), static_cast<uint8_t>(chars >> 8) };
        auto p = reinterpret_cast<const uint8_t*>(name.data());
        body.insert(body.end(), p, p + chars * 2);
        return GameMessage(2, 0, body);
    }

    void SendRaw(SOCKET s, const sockaddr_in6& to, const std::vector<uint8_t>& data)
    {
        sendto(s, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    }

    // Game messages that arrive (from the real game, through EOS P2P and our link) on our "game" socket.
    void GameReceiver(SOCKET s, sockaddr_in6 link)
    {
        uint8_t buf[4096];
        while (!g_stop)
        {
            int n = recv(s, reinterpret_cast<char*>(buf), sizeof(buf), 0);
            if (n == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAECONNRESET) continue;
                return;
            }
            for (int off = 0; off + 7 <= n;)
            {
                uint16_t seq, length, flags;
                memcpy(&seq, buf + off, 2);
                memcpy(&length, buf + off + 3, 2);
                memcpy(&flags, buf + off + 5, 2);
                uint8_t type = buf[off + 2];
                if (length < 7 || off + length > n) break;
                if (type == 2 && length >= 10)
                {
                    uint16_t chars;
                    memcpy(&chars, buf + off + 8, 2);
                    std::wstring name(reinterpret_cast<const wchar_t*>(buf + off + 10), (std::min)(static_cast<size_t>(chars), static_cast<size_t>((length - 10) / 2)));
                    Log(L"p2ppeer: <- HELLO from the game, name '%s'", name.c_str());
                    if (!buf[off + 7]) SendRaw(s, link, Hello(true));
                    ++g_hellos;
                }
                else if (type == 1) { if (g_heartbeats++ < 3) Log(L"p2ppeer: <- heartbeat seq %u", seq); }
                else if (g_other++ < 20) Log(L"p2ppeer: <- type 0x%02X seq %u, %u bytes", type, seq, length);
                if (flags & 0x8000) SendRaw(s, link, GameMessage(4, 0, {}, seq));
                off += length;
            }
        }
    }

    int RunP2PPeer(std::string gameUser, int seconds)
    {
        if (gameUser.empty())
        {
            auto found = lobbies::Search(std::string("pd1|") + kScid + "|SearchableGame");
            if (found.empty()) { Log(L"p2ppeer: no game lobby found; create a match first"); return 1; }
            gameUser = found.front().ownerId;
            Log(L"p2ppeer: using the owner of lobby %S", found.front().id.c_str());
        }
        p2p::Start([](const net::Message& m) { Log(L"p2ppeer: <- control message %u from %S", static_cast<unsigned>(m.type), m.peer.substr(0, 8).c_str()); });

        int rtt = -1;
        for (int i = 0; i < 5 && rtt < 0; ++i) rtt = p2p::Ping(gameUser, 4000);
        Log(rtt >= 0 ? L"p2ppeer: 1/4 PASS ping over EOS P2P: %d ms" : L"p2ppeer: 1/4 FAIL no pong over EOS P2P", rtt);

        net::Snapshot me = net::Local();
        auto body = me.Serialize();
        std::string templ = "PeerToHostUDP";
        body.push_back(static_cast<uint8_t>(templ.size()));
        body.insert(body.end(), templ.begin(), templ.end());
        net::Message reply;
        net::Snapshot acked;
        bool connected = p2p::Request(gameUser, net::MsgType::Connect, body, 20000, reply) && net::Snapshot::Parse(reply.body.data(), reply.body.size(), acked);
        if (!connected) { Log(L"p2ppeer: 2/4 FAIL no ack to CONNECT"); return 1; }
        Log(L"p2ppeer: 2/4 PASS connect -> ack from %s", acked.Describe().c_str());

        uint16_t linkPort = p2p::OpenLink(gameUser);
        SOCKET game = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        DWORD off = 0, timeout = 1000;
        setsockopt(game, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));
        setsockopt(game, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        sockaddr_in6 bindTo{};
        bindTo.sin6_family = AF_INET6;
        bindTo.sin6_port = htons(GetConfig().gamePort);
        if (!linkPort || bind(game, reinterpret_cast<sockaddr*>(&bindTo), sizeof(bindTo)) != 0) { Log(L"p2ppeer: local sockets failed (%d)", WSAGetLastError()); return 1; }
        auto link = net::GameSockAddr(net::InterfaceIp(), linkPort);
        std::thread receiver(GameReceiver, game, link);
        Sleep(500);

        SendRaw(game, link, Hello(false));
        for (int i = 0; i < 80 && !g_hellos; ++i) Sleep(100);
        Log(g_hellos ? L"p2ppeer: 3/4 PASS the game answered our HELLO over EOS P2P" : L"p2ppeer: 3/4 FAIL no HELLO back within 8 s");
        for (int i = 0; i < seconds; ++i)
        {
            SendRaw(game, link, GameMessage(1, 0, {}));
            Sleep(1000);
        }
        Log(L"p2ppeer: 4/4 %s %d heartbeat(s), %d other message(s) over %d s", g_heartbeats ? L"PASS" : L"FAIL", g_heartbeats.load(), g_other.load(), seconds);
        p2p::SendControl(gameUser, net::MsgType::Delete, net::NewNonce(), body);
        Sleep(1500);
        g_stop = true;
        receiver.join();
        closesocket(game);
        p2p::CloseLink(gameUser);
        return g_hellos && g_heartbeats ? 0 : 1;
    }

    // ---- watch: post lobby opened / players changed / full / closed to a Discord channel webhook ----

    std::map<std::string, std::string> ReadIni(const std::wstring& path)
    {
        std::map<std::string, std::string> out;
        FILE* f = nullptr;
        if (_wfopen_s(&f, path.c_str(), L"r") || !f) return out;
        char line[2048];
        while (fgets(line, sizeof(line), f))
        {
            std::string s = line;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
            auto eq = s.find('=');
            if (s.empty() || s[0] == ';' || s[0] == '#' || eq == std::string::npos) continue;
            out[s.substr(0, eq)] = s.substr(eq + 1);
        }
        fclose(f);
        return out;
    }

    int RunWatch(bool dryRun)
    {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring dir = exe;
        dir = dir.substr(0, dir.find_last_of(L'\\') + 1);
        auto ini = ReadIni(dir + L"lobbywatch.ini");
        std::wstring webhook = winrt::to_hstring(ini["webhook_url"]).c_str();
        int interval = ini["interval_seconds"].empty() ? 15 : (std::max)(10, atoi(ini["interval_seconds"].c_str()));
        if (webhook.empty() && !dryRun)
        {
            wprintf(L"Put webhook_url=<your Discord webhook URL> in %slobbywatch.ini, or run 'lobbytool watch dry' to only print messages.\n", dir.c_str());
            return 2;
        }
        auto post = [&](const std::string& text)
        {
            Log(L"watch: %S", text.c_str());
            if (!dryRun) discord::PostNow(webhook, text);
        };

        struct Seen
        {
            std::string name;
            uint32_t members = 0, max = 0;
            int misses = 0;
            bool full = false;
        };
        std::map<std::string, Seen> seen;
        std::string bucket = std::string("pd1|") + kScid + "|SearchableGame";
        Log(L"watch: checking for Phantom Dust lobbies every %d s%s", interval, dryRun ? L" (dry run: messages are only printed)" : L"");
        for (bool first = true;; first = false)
        {
            if (!lobbies::Available())
            {
                Log(L"watch: EOS not signed in, waiting");
                Sleep(interval * 1000);
                continue;
            }
            auto found = lobbies::Search(bucket, 100);
            std::set<std::string> present;
            for (auto& l : found)
            {
                if (l.memberCount == 0) continue; // abandoned: Epic keeps an empty lobby around for a while after its host leaves
                present.insert(l.id);
                std::string name = discord::LobbyProperty(l, L"gamename");
                std::string match = discord::MatchDescription(l);
                auto it = seen.find(l.id);
                if (it == seen.end())
                {
                    seen[l.id] = Seen{ name, l.memberCount, l.maxMembers };
                    // Lobbies that already exist when the watcher starts are listed quietly in the console only.
                    if (first) Log(L"watch: already open: %S's lobby (%u/%u)", name.c_str(), l.memberCount, l.maxMembers);
                    else post("\xF0\x9F\x9F\xA2 **" + discord::Name(name) + "** opened a Phantom Dust lobby \xC2\xB7 " + (match.empty() ? "" : match + " \xC2\xB7 ") +
                              std::to_string(l.memberCount) + "/" + std::to_string(l.maxMembers) + " players");
                    continue;
                }
                Seen& s = it->second;
                s.misses = 0;
                if (s.full || s.members != l.memberCount)
                {
                    s.full = false;
                    s.members = l.memberCount;
                    post("\xF0\x9F\x91\xA5 **" + discord::Name(s.name) + "**'s lobby \xC2\xB7 " + std::to_string(l.memberCount) + "/" + std::to_string(l.maxMembers) + " players");
                }
            }
            for (auto it = seen.begin(); it != seen.end();)
            {
                Seen& s = it->second;
                if (present.count(it->first) || ++s.misses < 2)
                {
                    ++it;
                    continue;
                }
                // Gone from search twice: Epic also hides full lobbies from search, so ask for it by id before calling it closed.
                auto direct = lobbies::Read(it->first);
                if (direct && direct->memberCount > 0)
                {
                    if (!s.full && direct->memberCount >= direct->maxMembers)
                    {
                        s.full = true;
                        s.members = direct->memberCount;
                        post("\xF0\x9F\x8E\xAE **" + discord::Name(s.name) + "**'s lobby is full \xC2\xB7 " + std::to_string(direct->memberCount) + "/" +
                             std::to_string(direct->maxMembers) + " players");
                    }
                    s.misses = 0;
                    ++it;
                    continue;
                }
                post("\xF0\x9F\x94\xB4 **" + discord::Name(s.name) + "**'s lobby closed");
                it = seen.erase(it);
            }
            Sleep(interval * 1000);
        }
    }

    bool WaitForSignIn()
    {
        for (int i = 0; i < 300 && !lobbies::Available(); ++i) Sleep(100);
        if (!lobbies::Available()) Log(L"lobbytool: EOS sign-in did not finish in 30 s");
        return lobbies::Available();
    }
}

int wmain(int argc, wchar_t** argv)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    std::wstring command = argc > 1 ? argv[1] : L"";
    if (command != L"search" && command != L"show" && command != L"join" && command != L"host" && command != L"p2ppeer" && command != L"watch")
    {
        wprintf(L"usage: lobbytool search [template] | show <lobby id> | join <lobby id> [seconds] | host <lobby id> [seconds]\n"
                L"       lobbytool watch [dry]   post lobby opened/closed to Discord (webhook_url in lobbywatch.ini; dry = print only)\n"
                L"       lobbytool p2ppeer [seconds] [force] [game user id]   (fake remote player over EOS P2P; force = relays only)\n");
        return 2;
    }
    if (command == L"p2ppeer")
        for (int i = 2; i < argc; ++i)
            if (_wcsicmp(argv[i], L"force") == 0) g_config.p2pRelay = L"force";
    eos::Start();
    if (!WaitForSignIn()) return 1;
    Log(L"lobbytool: signed in as product user %s", eos::LocalUserId().c_str());

    if (command == L"watch") return RunWatch(argc > 2 && _wcsicmp(argv[2], L"dry") == 0);

    if (command == L"p2ppeer")
    {
        int seconds = 20;
        std::string user;
        for (int i = 2; i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"force") == 0) continue;
            if (iswdigit(argv[i][0]) && wcslen(argv[i]) < 6) seconds = _wtoi(argv[i]);
            else user = winrt::to_string(argv[i]);
        }
        return RunP2PPeer(user, seconds);
    }

    if (command == L"search")
    {
        std::string templ = argc > 2 ? winrt::to_string(argv[2]) : "SearchableGame";
        std::string bucket = std::string("pd1|") + kScid + "|" + templ;
        auto found = lobbies::Search(bucket);
        Log(L"lobbytool: %zu lobby(s) in bucket %S", found.size(), bucket.c_str());
        for (auto& l : found) Print(l);
        return 0;
    }

    if (argc < 3)
    {
        wprintf(L"lobby id missing\n");
        return 2;
    }
    std::string id = winrt::to_string(argv[2]);
    if (command == L"show")
    {
        auto l = lobbies::Read(id);
        if (!l)
        {
            Log(L"lobbytool: lobby %S not found", id.c_str());
            return 1;
        }
        Print(*l);
        return 0;
    }

    // join: look like a pdnet player (see xsapi\mpsd.h for the member attributes).
    int seconds = argc > 3 ? _wtoi(argv[3]) : 60;
    if (command == L"host")
    {
        // Self-test of the lobby layer: create with a long (split) attribute, find it by bucket and by id, update, destroy.
        std::string bucket = std::string("pd1|") + kScid + "|lobbytool-test";
        std::string longValue;
        for (int i = 0; i < 130; ++i) longValue += "0123456789"; // 1300 bytes -> 3 pieces
        lobbies::Attributes attrs = { { "PD_VERSION", "1" }, { "PD_NAME", "selftest" } };
        lobbies::Split(attrs, "PD_PROPS", longValue);
        auto created = lobbies::Create(id, bucket, 4, attrs, { { "PD_XUID", "1" }, { "PD_NAME", "lobbytool" } });
        if (!created) return 1;
        Print(*created);
        bool roundTrip = lobbies::Joined(created->attributes, "PD_PROPS") == longValue;
        auto found = lobbies::Search(bucket);
        auto byId = lobbies::Read(id);
        bool updated = lobbies::Update(id, lobbies::Changes{ { { "PD_CLOSED", "1" } }, { "PD_NAME" } }, lobbies::Changes{ { { "PD_ADDR", "abc" } }, {} });
        auto after = lobbies::Read(id);
        bool updateSeen = after && after->attributes.count("PD_CLOSED") && !after->attributes.count("PD_NAME") && !after->members.empty() && after->members[0].attributes.count("PD_ADDR");
        Log(L"selftest: split round trip %s, search found %zu, read by id %s, update %s / visible %s", roundTrip ? L"ok" : L"FAIL", found.size(), byId ? L"ok" : L"FAIL",
            updated ? L"ok" : L"FAIL", updateSeen ? L"ok" : L"FAIL");
        if (seconds > 0)
        {
            Log(L"lobbytool: lobby stays up for %d s", seconds);
            Sleep(seconds * 1000);
        }
        Log(L"selftest: destroy %s", lobbies::Destroy(id) ? L"ok" : L"FAIL");
        return 0;
    }
    lobbies::SetListener([](const std::string& lobbyId, lobbies::Event e, const std::string& user)
    {
        static const wchar_t* names[] = { L"lobby updated", L"member updated", L"member joined", L"member left", L"promoted", L"closed", L"kicked" };
        Log(L"lobbytool: event %s (lobby %S, user %S)", names[static_cast<int>(e)], lobbyId.c_str(), user.c_str());
    });
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    int64_t ticks = (static_cast<int64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    lobbies::Attributes me = {
        { "PD_XUID", "2533274790395903" }, { "PD_NAME", "lobbytool" }, { "PD_STATUS", "3" /* Active */ }, { "PD_INIT", "0" }, { "PD_JOIN", std::to_string(ticks) },
        // A well-formed pdnet snapshot (game 43213, control 43212) at 192.0.2.1, a TEST-NET address nobody answers on,
        // so the game can parse it and its connection attempt simply times out.
        { "PD_ADDR", "UEROMQEAqM2ozAAAAAAAAAAAAAD//8AAAgEAAAAB2x7nAQ==" },
    };
    auto l = lobbies::Join(id, me);
    if (!l)
    {
        Log(L"lobbytool: could not join %S", id.c_str());
        return 1;
    }
    Print(*l);
    Log(L"lobbytool: staying for %d s", seconds);
    for (int i = 0; i < seconds; ++i) Sleep(1000);
    if (auto after = lobbies::Read(id)) Print(*after);
    lobbies::Leave(id);
    Log(L"lobbytool: left");
    return 0;
}
