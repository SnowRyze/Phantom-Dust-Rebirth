#include "pch.h"
#include "log.h"
#include "logscrub.h"
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <share.h>
#include <appmodel.h>

namespace pdnet
{
    const std::wstring& LocalStatePath()
    {
        static std::wstring path = []
        {
            // Built from the package family name first: in injected mode the log is written before the game's app
            // APIs are ready, and calling ApplicationData then can break the game's start-up.
            UINT32 length = 0;
            if (GetCurrentPackageFamilyName(&length, nullptr) == ERROR_INSUFFICIENT_BUFFER && length)
            {
                std::wstring family(length, L'\0');
                wchar_t profile[MAX_PATH];
                DWORD n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
                if (GetCurrentPackageFamilyName(&length, family.data()) == ERROR_SUCCESS && n && n < MAX_PATH)
                {
                    family.resize(length - 1);
                    std::wstring local = std::wstring(profile) + L"\\AppData\\Local\\Packages\\" + family + L"\\LocalState";
                    DWORD attributes = GetFileAttributesW(local.c_str());
                    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) return local;
                }
            }
            try
            {
                return std::wstring{ winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path() };
            }
            catch (...)
            {
                return std::wstring{};
            }
        }();
        return path;
    }

    namespace
    {
        std::mutex g_nameLock;
        std::wstring g_logName, g_computer;

        // The kit's pdnet.ini (next to pdnet.dll) and the in-game Settings' settings.ini.
        std::wstring FirstLogName()
        {
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&FirstLogName), &self);
            wchar_t module[MAX_PATH] = {};
            GetModuleFileNameW(self, module, MAX_PATH);
            std::wstring kit = module;
            kit = kit.substr(0, kit.find_last_of(L'\\') + 1);
            std::vector<std::wstring> files{ kit + L"pdnet.ini" };
            if (!LocalStatePath().empty()) files.push_back(LocalStatePath() + L"\\pdnet\\settings.ini");
            return logscrub::ReadName(files);
        }
    }

    void SetLogName(const std::wstring& name)
    {
        std::lock_guard guard(g_nameLock);
        g_logName = name.empty() ? L"player" : name.substr(0, 16);
    }

    void Log(const wchar_t* fmt, ...)
    {
        static std::mutex lock;
        static FILE* file = nullptr;

        wchar_t body[2048];
        va_list args;
        va_start(args, fmt);
        _vsnwprintf_s(body, _TRUNCATE, fmt, args);
        va_end(args);

        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t stamped[2200];
        swprintf_s(stamped, L"%02u:%02u:%02u.%03u [%5lu] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId(), body);
        std::wstring text = stamped;
        {
            std::lock_guard guard(g_nameLock);
            if (g_logName.empty()) g_logName = FirstLogName();
            if (g_computer.empty()) g_computer = logscrub::ComputerName();
            logscrub::Scrub(text, g_logName, g_computer); // no Windows user or computer name in the log (the user, 2026-11-22)
        }
        const wchar_t* line = text.c_str();

        OutputDebugStringW(L"[pdnet] ");
        OutputDebugStringW(line);

        std::lock_guard guard(lock);
        if (!file && !LocalStatePath().empty())
        {
            // Shared so the log can be read while the game is running.
            file = _wfsopen((LocalStatePath() + L"\\pdnet.log").c_str(), L"a, ccs=UTF-8", _SH_DENYNO);
        }
        if (file)
        {
            fputws(line, file);
            fflush(file);
        }
    }
}
