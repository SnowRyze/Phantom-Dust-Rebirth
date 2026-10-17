#include "pch.h"
#include "log.h"
#include <cstdarg>
#include <cstdio>
#include <share.h>

namespace pdnet
{
    const std::wstring& LocalStatePath()
    {
        static std::wstring path = []
        {
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
