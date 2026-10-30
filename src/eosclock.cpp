#include "pch.h"
#include "eosclock.h"
#include "log.h"
#include <sys/timeb.h>
#include <ctime>
#include <fstream>

namespace pdnet
{
    namespace
    {
        long long g_offset = 0; // seconds to add (set once, before EOS starts)

        ULONGLONG Shifted()
        {
            FILETIME now;
            GetSystemTimeAsFileTime(&now);
            return ((static_cast<ULONGLONG>(now.dwHighDateTime) << 32) | now.dwLowDateTime) + g_offset * 10000000LL;
        }

        FILETIME ToFileTime(ULONGLONG t) { return { static_cast<DWORD>(t), static_cast<DWORD>(t >> 32) }; }

        VOID WINAPI EosGetSystemTimeAsFileTime(LPFILETIME out) { *out = ToFileTime(Shifted()); }

        VOID WINAPI EosGetSystemTime(LPSYSTEMTIME out)
        {
            FILETIME t = ToFileTime(Shifted());
            FileTimeToSystemTime(&t, out);
        }

        VOID WINAPI EosGetLocalTime(LPSYSTEMTIME out)
        {
            FILETIME utc = ToFileTime(Shifted()), local;
            FileTimeToLocalFileTime(&utc, &local);
            FileTimeToSystemTime(&local, out);
        }

        __time64_t __cdecl EosTime64(__time64_t* out)
        {
            __time64_t t = _time64(nullptr) + g_offset;
            if (out) *out = t;
            return t;
        }

        void __cdecl EosFtime64(__timeb64* out)
        {
            if (!out) return;
            _ftime64_s(out);
            out->time += g_offset;
        }

        int PatchTimeImports(HMODULE module)
        {
            struct Replacement { const char* name; void* with; };
            const Replacement kernel[] = { { "GetSystemTimeAsFileTime", reinterpret_cast<void*>(&EosGetSystemTimeAsFileTime) },
                                           { "GetSystemTime", reinterpret_cast<void*>(&EosGetSystemTime) },
                                           { "GetLocalTime", reinterpret_cast<void*>(&EosGetLocalTime) } };
            const Replacement crt[] = { { "_time64", reinterpret_cast<void*>(&EosTime64) },
                                        { "_ftime64", reinterpret_cast<void*>(&EosFtime64) } };
            auto base = reinterpret_cast<uint8_t*>(module);
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
            auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            int patched = 0;
            for (auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); dir.VirtualAddress && desc->Name; ++desc)
            {
                auto dll = reinterpret_cast<const char*>(base + desc->Name);
                const Replacement* list = nullptr;
                size_t count = 0;
                if (_stricmp(dll, "kernel32.dll") == 0 || _strnicmp(dll, "api-ms-win-core-sysinfo-", 24) == 0) list = kernel, count = std::size(kernel);
                else if (_strnicmp(dll, "api-ms-win-crt-time-", 20) == 0 || _stricmp(dll, "ucrtbase.dll") == 0) list = crt, count = std::size(crt);
                if (!list) continue;
                auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
                auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
                for (; names->u1.AddressOfData; ++names, ++slots)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                    auto name = reinterpret_cast<const char*>(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name);
                    for (size_t i = 0; i < count; ++i)
                    {
                        if (strcmp(name, list[i].name) != 0) continue;
                        DWORD old;
                        VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &old);
                        slots->u1.Function = reinterpret_cast<ULONG_PTR>(list[i].with);
                        VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), old, &old);
                        ++patched;
                    }
                }
            }
            return patched;
        }
    }

    long long CorrectEosClock(HMODULE eosModule)
    {
        std::ifstream in(LocalStatePath() + L"\\pdnet\\clock-offset.txt");
        long long offset = 0;
        if (!(in >> offset))
        {
            Log(L"clock: no offset from pdlaunch - EOS uses this PC's clock as it is");
            return 0;
        }
        if (offset > -60 && offset < 60)
        {
            Log(L"clock: this PC's clock is within %lld s of Epic's", offset < 0 ? -offset : offset);
            return 0;
        }
        g_offset = offset;
        int patched = PatchTimeImports(eosModule);
        Log(L"clock: this PC's clock is %lld s %s Epic's - EOS sees the corrected time (%d imports)", offset < 0 ? -offset : offset,
            offset < 0 ? L"ahead of" : L"behind", patched);
        return offset;
    }
}
