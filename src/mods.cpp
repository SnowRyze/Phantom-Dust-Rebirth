#include "pch.h"
#include "mods.h"
#include "config.h"
#include "log.h"
#include <fstream>

namespace pdnet::mods
{
    namespace
    {
        std::mutex g_resultsLock;
        std::vector<std::string> g_results;
        bool g_lobbyStart = false;

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

    const std::string& LobbyTag()
    {
        static std::string tag = []
        {
            std::string out;
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
            Log(L"mods: gameplay mods this start (what lobbies compare): %S%s", list.empty() ? "none" : list.c_str(), g_lobbyStart ? L" - a lobby's, as asked when joining it" : L"");
            return out;
        }();
        return tag;
    }

    size_t Count(const std::string& lobbyTag) { return lobbyTag.empty() ? 0 : Fields(lobbyTag, ',').size(); }

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
