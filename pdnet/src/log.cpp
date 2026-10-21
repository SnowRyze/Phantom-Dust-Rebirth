#include "pch.h"
#include "log.h"
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
        wchar_t line[2200];
        swprintf_s(line, L"%02u:%02u:%02u.%03u [%5lu] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId(), body);

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
