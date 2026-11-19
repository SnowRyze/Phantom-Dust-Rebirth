#pragma once
// Logs never show the PC's own names (the user, 2026-11-22): the Windows user name in paths ("C:\Users\<name>\...",
// Wine's "Z:\home\<name>\...") and the computer name are replaced with the player's in-game name (display_name).
// Header only, for pdnet.dll (log.cpp, discord.cpp) and pdlaunch.exe alike.
#include <windows.h>
#include <cwctype>
#include <string>
#include <vector>

namespace pdnet::logscrub
{
    // display_name= from these ini files (a later file wins: pdnet.ini, then settings.ini), or "player".
    inline std::wstring ReadName(const std::vector<std::wstring>& iniFiles)
    {
        std::wstring name;
        for (const auto& path : iniFiles)
        {
            HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (f == INVALID_HANDLE_VALUE) continue;
            char buffer[16384];
            DWORD got = 0;
            if (!ReadFile(f, buffer, sizeof(buffer) - 1, &got, nullptr)) got = 0;
            CloseHandle(f);
            std::string text(buffer, got), line;
            for (size_t start = 0; start < text.size();)
            {
                size_t end = text.find('\n', start);
                line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
                start = end == std::string::npos ? text.size() : end + 1;
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
                if (line.rfind("display_name=", 0) != 0 || line.size() <= 13) continue;
                std::string value = line.substr(13);
                int n = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
                std::wstring wide(n, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), wide.data(), n);
                if (!wide.empty()) name = wide.substr(0, 16);
            }
        }
        return name.empty() ? L"player" : name;
    }

    inline std::wstring ComputerName()
    {
        wchar_t pc[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
        return GetComputerNameW(pc, &n) ? std::wstring(pc, n) : std::wstring();
    }

    inline bool SameText(const wchar_t* a, const std::wstring& b)
    {
        for (size_t i = 0; i < b.size(); ++i)
            if (!a[i] || towlower(a[i]) != towlower(b[i])) return false;
        return true;
    }

    // Replaces the user folder's name after "\Users\" / "\home\" / "/home/" (not Public, Default, All Users) and every
    // whole-word copy of the computer name with `name`.
    inline void Scrub(std::wstring& line, const std::wstring& name, const std::wstring& computer)
    {
        static const wchar_t* kRoots[] = { L"\\users\\", L"\\home\\", L"/home/" };
        for (const wchar_t* root : kRoots)
        {
            std::wstring r = root;
            for (size_t at = 0; at + r.size() < line.size();)
            {
                if (!SameText(line.c_str() + at, r)) { ++at; continue; }
                size_t from = at + r.size(), to = from;
                while (to < line.size() && line[to] != L'\\' && line[to] != L'/' && line[to] != L'\n') ++to;
                std::wstring user = line.substr(from, to - from);
                bool keep = user.empty() || _wcsicmp(user.c_str(), L"Public") == 0 || _wcsicmp(user.c_str(), L"Default") == 0 ||
                            _wcsicmp(user.c_str(), L"All Users") == 0 || user == name;
                if (!keep) line.replace(from, to - from, name);
                at = from + (keep ? user.size() : name.size());
            }
        }
        if (computer.size() >= 2)
            for (size_t at = 0; at + computer.size() <= line.size();)
            {
                bool word = (at == 0 || !iswalnum(line[at - 1])) &&
                            (at + computer.size() == line.size() || !(iswalnum(line[at + computer.size()]) || line[at + computer.size()] == L'-'));
                if (word && SameText(line.c_str() + at, computer)) { line.replace(at, computer.size(), name); at += name.size(); }
                else ++at;
            }
    }

    // A file-name-safe form of the name (for "pdnet-log-<name>-<date>.txt").
    inline std::wstring FileSafe(const std::wstring& name)
    {
        std::wstring out;
        for (wchar_t c : name) out += iswalnum(c) || c == L'-' || c == L'_' ? c : L'_';
        return out.empty() ? L"player" : out;
    }
}
