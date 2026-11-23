#include "pch.h"
#include "mods.h"
#include "config.h"
#include "gamepatches.h"
#include "pdpl.h"
#include "log.h"
#include <algorithm>
#include <condition_variable>
#include <fstream>

namespace pdnet::mods
{
    namespace
    {
        std::mutex g_resultsLock;
        std::vector<std::string> g_results;
        std::atomic<bool> g_lobbyStart{ false };
        std::mutex g_tagLock;
        std::string g_tag;        // guarded by g_tagLock
        bool g_tagRead = false;   // "
        std::mutex g_liveLock;    // one live change at a time

        std::vector<std::string> Fields(const std::string& line, char sep)
        {
            std::vector<std::string> out(1);
            for (char c : line)
                if (c == sep) out.emplace_back();
                else out.back() += c;
            return out;
        }

        // A tag's parts must not hold the separators.
        std::string Clean(std::string s)
        {
            for (char& c : s) if (c == ',' || c == ':' || c == '\t' || c == '\r' || c == '\n') c = '_';
            return s;
        }
    }

    namespace
    {
        std::string ReadTag()
        {
            std::string out;
            g_lobbyStart = false;
            if (!GetModuleHandleW(L"pd_loader_core.dll")) return out; // no PDPL this start: no mods loaded
            std::ifstream in(LocalStatePath() + L"\\pdnet\\mods-state.txt", std::ios::binary);
            std::string line;
            std::vector<std::string> names;
            while (std::getline(in, line))
            {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                auto f = Fields(line, '\t');
                if (f[0] == "lobbystart") g_lobbyStart = true;
                // mod id on|off source name version author online touches plugins files problem path after before description sha tag
                if (f[0] != "mod" || f.size() < 17) continue;
                bool loaded = f[2] == "on" && !f[12].empty() && f[11].rfind("has plugins", 0) != 0;
                if (!loaded || f[7] == "local" || f[16].empty()) continue;
                out += (out.empty() ? "" : ",") + Clean(f[1]) + ":" + Clean(f[16]);
                if (f[3] == "catalog" && f.size() > 17 && !f[17].empty()) out += ":" + Clean(f[17]);
                names.push_back(f[1]);
            }
            std::string list;
            for (auto& n : names) list += (list.empty() ? "" : ", ") + n;
            Log(L"mods: gameplay mods in use (what lobbies compare): %S%s", list.empty() ? "none" : list.c_str(), g_lobbyStart ? L" - a lobby's, as asked when joining it" : L"");
            return out;
        }

        std::string LiveFile(const wchar_t* name)
        {
            std::ifstream in(LocalStatePath() + L"\\pdnet\\" + name, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in), {});
        }

        void WriteLiveFile(const wchar_t* name, const std::string& text)
        {
            std::wstring path = LocalStatePath() + L"\\pdnet\\" + name, tmp = path + L".tmp";
            FILE* f = nullptr;
            if (_wfopen_s(&f, tmp.c_str(), L"wb") || !f) return;
            fwrite(text.data(), 1, text.size(), f);
            fclose(f);
            for (int i = 0; i < 400 && !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING); ++i) Sleep(5); // (pdlaunch may be reading it)
        }

        // One live change, start to end (pdlaunch.cpp, WatchLiveMods, is the other side of the files).
        Live RunLive(const std::string& reason, std::string& why)
        {
            if (!LiveAvailable()) { why = "PDPL here can't change mods while the game runs"; return Live::NotNow; }
            if (!gamepatches::CanSwapMods()) { why = "a match or its lobby is on screen"; return Live::NotNow; }
            static std::atomic<uint32_t> counter{ GetTickCount() };
            std::string number = std::to_string(++counter);
            Log(L"mods: live: asking pdlaunch (%S, request %S)", reason.c_str(), number.c_str());
            WriteLiveFile(L"mods-live.txt", number);
            bool working = false, unmounted = false;
            Live result = Live::Failed;
            ULONGLONG start = GetTickCount64();
            for (;;)
            {
                Sleep(50);
                std::string reply = LiveFile(L"mods-reply.txt");
                std::string state = reply.rfind(number + "\t", 0) == 0 ? reply.substr(number.size() + 1) : "";
                working = working || !state.empty();
                ULONGLONG waited = GetTickCount64() - start;
                if (!working && waited > 4000) { DeleteFileW((LocalStatePath() + L"\\pdnet\\mods-live.txt").c_str()); why = "pdlaunch didn't answer"; break; }
                if (waited > 300000) { why = "pdlaunch took too long"; break; }
                if (state == "ready" && !unmounted)
                {
                    std::string bad;
                    if (!gamepatches::CanSwapMods()) bad = "a match or its lobby came on screen";
                    else if (!pdpl::UnmountArchives(bad)) pdpl::MountArchives(); // some may be out already
                    else unmounted = true;
                    WriteLiveFile(L"mods-go.txt", number + (unmounted ? "\tgo" : "\tno"));
                    if (!unmounted) { why = bad; result = Live::NotNow; break; }
                }
                else if (state == "done") { result = Live::Done; break; }
                else if (state.rfind("restart", 0) == 0) { why = state.size() > 8 ? state.substr(8) : ""; result = Live::Restart; break; }
                else if (state == "failed") { why = "pdlaunch couldn't (see pdlaunch.log)"; break; }
            }
            if (unmounted) pdpl::MountArchives(); // whatever is in the folder now: the new mods, or the old ones
            if (result == Live::Done)
            {
                {
                    std::lock_guard guard(g_tagLock);
                    g_tag = ReadTag();
                    g_tagRead = true;
                }
                gamepatches::ModsSwapped();
            }
            Log(L"mods: live: %s%S (%llu ms)", result == Live::Done ? L"done" : result == Live::Restart ? L"at the next game start, these load only then: " : L"not done: ", why.c_str(), GetTickCount64() - start);
            return result;
        }
    }

    std::string LobbyTag()
    {
        std::lock_guard guard(g_tagLock);
        if (!g_tagRead) g_tag = ReadTag(), g_tagRead = true;
        return g_tag;
    }

    bool LiveAvailable() { return pdpl::LiveReady(); }

    Live ApplyLive(const char* reason, DWORD timeoutMs, std::string& why)
    {
        struct Shared { std::mutex lock; std::condition_variable ready; bool finished = false; Live result = Live::Failed; std::string why; };
        auto shared = std::make_shared<Shared>();
        std::thread([shared, text = std::string(reason)]
        {
            std::string w;
            Live r;
            {
                std::lock_guard one(g_liveLock);
                r = RunLive(text, w);
            }
            std::lock_guard guard(shared->lock);
            shared->result = r;
            shared->why = w;
            shared->finished = true;
            shared->ready.notify_all();
        }).detach();
        std::unique_lock guard(shared->lock);
        if (!shared->ready.wait_for(guard, std::chrono::milliseconds(timeoutMs), [&] { return shared->finished; })) return Live::Busy;
        why = shared->why;
        return shared->result;
    }

    size_t Count(const std::string& lobbyTag) { return lobbyTag.empty() ? 0 : Fields(lobbyTag, ',').size(); }

    bool Differs(const std::string& lobbyTag)
    {
        auto sorted = [](const std::string& tag)
        {
            std::vector<std::string> v;
            if (!tag.empty()) v = Fields(tag, ',');
            std::sort(v.begin(), v.end());
            return v;
        };
        return sorted(lobbyTag) != sorted(LobbyTag());
    }

    bool LobbyStart()
    {
        LobbyTag();
        return g_lobbyStart;
    }

    bool RequestForNextStart(const std::string& lobbyId, const std::string& lobbyTag, std::string& why)
    {
        if (!GetConfig().lobbyModsAuto)
        {
            why = "lobby_mods=off";
            return false;
        }
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        uint64_t now = ((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) / 10000000ull - 11644473600ull; // Unix seconds (pdlaunch reads it)
        std::string text = "lobby\t" + lobbyId + "\ntime\t" + std::to_string(now) + "\n";
        if (!lobbyTag.empty())
            for (const auto& entry : Fields(lobbyTag, ','))
            {
                auto p = Fields(entry, ':');
                if (p.size() < 2 || p[0].empty()) continue;
                text += "mod\t" + p[0] + "\t" + p[1] + (p.size() > 2 ? "\t" + p[2] : "") + "\n";
            }
        std::wstring path = LocalStatePath() + L"\\pdnet\\lobby-mods.txt";
        FILE* f = nullptr;
        if (_wfopen_s(&f, path.c_str(), L"wb") || !f)
        {
            why = "couldn't write lobby-mods.txt";
            return false;
        }
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
        Log(L"mods: lobby %S has other mods (%S) - set up for the next game start (lobby-mods.txt)", lobbyId.c_str(), lobbyTag.empty() ? "none" : lobbyTag.c_str());
        return true;
    }

    namespace
    {
        std::mutex g_joinLock;
        std::string g_joinText;                    // guarded by g_joinLock
        std::atomic<uint32_t> g_joinNumber{ 0 };   // the lobby being loaded for: a newer one (or leaving) ends an older
        std::atomic<bool> g_joinLoading{ false };

        void JoinText(const std::string& text)
        {
            std::lock_guard guard(g_joinLock);
            g_joinText = text;
        }

        // What of a lobby's mods isn't in use here.
        std::string Missing(const std::string& lobbyTag)
        {
            std::vector<std::string> mine;
            if (std::string own = LobbyTag(); !own.empty()) mine = Fields(own, ',');
            std::string out;
            if (!lobbyTag.empty())
                for (const auto& entry : Fields(lobbyTag, ','))
                    if (std::find(mine.begin(), mine.end(), entry) == mine.end()) out += (out.empty() ? "" : ", ") + Fields(entry, ':')[0];
            return out;
        }
    }

    bool JoinLobby(const std::string& lobbyId, const std::string& lobbyTag)
    {
        if (!LiveAvailable()) return false;
        uint32_t number = ++g_joinNumber;
        JoinText("");
        g_joinLoading = true;
        Log(L"mods: lobby %S has other mods (%S; here %S) - joining, they load in the lobby", lobbyId.c_str(), lobbyTag.empty() ? "none" : lobbyTag.c_str(),
            LobbyTag().empty() ? "none" : LobbyTag().c_str());
        std::thread([number, lobbyId, lobbyTag]
        {
            auto wanted = [number] { return g_joinNumber == number; };
            for (int attempt = 1; wanted(); ++attempt)
            {
                std::string why;
                Live result = Live::Done;
                if (Differs(lobbyTag))
                {
                    if (!RequestForNextStart(lobbyId, lobbyTag, why))
                    {
                        JoinText("Not loaded: " + why + ". B: leave");
                        return;
                    }
                    result = ApplyLive("joining a lobby", 900000, why);
                }
                if (!wanted()) return;
                if (!Differs(lobbyTag))
                {
                    Log(L"mods: the lobby's mods are loaded (try %d)", attempt);
                    g_joinLoading = false;
                    return;
                }
                if (result == Live::Restart)
                {
                    RequestForNextStart(lobbyId, lobbyTag, why); // for that restart
                    JoinText("Some load only at game start. Press B to leave,\nrestart the game and join again.");
                    Log(L"mods: the lobby's mods need a game restart: %S", why.c_str());
                    return;
                }
                std::string missing = Missing(lobbyTag);
                if (attempt > 1 || result != Live::NotNow)
                    JoinText((missing.empty() ? "Not loaded yet" : "Couldn't get " + missing.substr(0, 40)) + ". Trying again...\nB: leave");
                Log(L"mods: the lobby's mods aren't all here (try %d): %S %S", attempt, missing.c_str(), why.c_str());
                for (int i = 0; i < (result == Live::NotNow ? 10 : 100) && wanted(); ++i) Sleep(100);
            }
        }).detach();
        return true;
    }

    bool LobbyLoading() { return g_joinLoading; }

    std::string LobbyLoadingText()
    {
        std::lock_guard guard(g_joinLock);
        return g_joinText;
    }

    void LobbyLeft()
    {
        uint32_t number = ++g_joinNumber;
        bool loading = g_joinLoading.exchange(false);
        if (loading) Log(L"mods: lobby left while its mods were loading");
        // This PC's own mods again, once out of the lobby (a restart the lobby needs keeps its request: nothing was changed).
        std::thread([number]
        {
            for (int i = 0; i < 300; ++i)
            {
                Sleep(1000);
                if (g_joinNumber != number) return; // another lobby already
                std::lock_guard one(g_liveLock); // after a change still going on
                LobbyTag();
                if (!g_lobbyStart) return;
                if (!gamepatches::CanSwapMods()) continue;
                DeleteFileW((LocalStatePath() + L"\\pdnet\\lobby-mods.txt").c_str());
                std::string why;
                if (RunLive("out of the lobby: this PC's own mods", why) == Live::Done) return;
            }
        }).detach();
    }

    namespace { std::atomic<ULONGLONG> g_listOpenedAt{ 0 }; }
    void ListOpened() { g_listOpenedAt = GetTickCount64(); }
    bool ListJustOpened() { ULONGLONG at = g_listOpenedAt; return at && GetTickCount64() - at < 3000; }

    void SetSearchResults(std::vector<std::string> tags)
    {
        std::lock_guard guard(g_resultsLock);
        g_results = std::move(tags);
    }

    std::string SearchResultTag(size_t index)
    {
        std::lock_guard guard(g_resultsLock);
        return index < g_results.size() ? g_results[index] : std::string();
    }
}
