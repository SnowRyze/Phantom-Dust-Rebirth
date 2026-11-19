#include "pch.h"
#include "pdpl.h"
#include "log.h"
#include <cstdio>
#include <share.h>
#include <thread>
#include <tlhelp32.h>
#pragma comment(lib, "user32.lib") // ShowWindow

namespace pdnet::pdpl
{
    namespace
    {
        HANDLE g_inRead = nullptr, g_inWrite = nullptr;   // PDPL's standard input: nothing is ever written
        HANDLE g_outRead = nullptr, g_outWrite = nullptr; // PDPL's standard output and error
        std::atomic<bool> g_prepared{ false };
        std::atomic<uint32_t> g_lines{ 0 }, g_opens{ 0 };

        // PDPL colours its messages with ANSI escapes ("\033[32m"): left out of the log.
        std::string WithoutEscapes(const std::string& s)
        {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[')
                {
                    i += 2;
                    while (i < s.size() && !(s[i] >= '@' && s[i] <= '~')) ++i; // up to the final letter
                    continue;
                }
                if (s[i] != '\r') out += s[i];
            }
            return out;
        }

        // Copies PDPL's output into LocalState\pdnet\pdpl.log, one timestamped line each (the last start's log is kept
        // as pdpl.prev.log). Its own messages (not the CreateFile2 lines, one per file the game opens) also go to
        // pdnet.log, the first 200 of them. PDPL's output is a pipe, so its C runtime buffers it: lines arrive in blocks.
        void ReadOutput()
        {
            std::wstring dir = LocalStatePath() + L"\\pdnet";
            CreateDirectoryW(dir.c_str(), nullptr);
            MoveFileExW((dir + L"\\pdpl.log").c_str(), (dir + L"\\pdpl.prev.log").c_str(), MOVEFILE_REPLACE_EXISTING);
            FILE* file = _wfsopen((dir + L"\\pdpl.log").c_str(), L"wb", _SH_DENYNO);
            std::string pending;
            char buf[8192];
            DWORD got = 0;
            uint32_t told = 0;
            while (ReadFile(g_outRead, buf, sizeof(buf), &got, nullptr) && got)
            {
                pending.append(buf, got);
                SYSTEMTIME t;
                GetLocalTime(&t);
                char stamp[16];
                sprintf_s(stamp, "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
                for (size_t end; (end = pending.find('\n')) != std::string::npos;)
                {
                    std::string line = WithoutEscapes(pending.substr(0, end));
                    pending.erase(0, end + 1);
                    if (line.empty()) continue;
                    ++g_lines;
                    bool open = line.rfind("CreateFile2", 0) == 0;
                    if (open) ++g_opens;
                    if (file)
                    {
                        fputs(stamp, file);
                        fputs(line.c_str(), file);
                        fputc('\n', file);
                    }
                    if (!open && line != "> " && told < 200)
                    {
                        ++told;
                        Log(L"pdpl: %S", line.c_str());
                    }
                }
                if (file) fflush(file);
            }
            if (file) fclose(file);
            Log(L"pdpl: its output closed (%u lines, %u file opens)", g_lines.load(), g_opens.load());
        }

        // CPU time (ms) used so far by the threads that started in pd_loader_core.dll.
        void LogThreadTimes(HMODULE module, const wchar_t* when)
        {
            MODULEINFO info{};
            if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info))) return;
            using QueryFn = LONG(NTAPI*)(HANDLE, int, void*, ULONG, ULONG*);
            auto query = reinterpret_cast<QueryFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (!query || snapshot == INVALID_HANDLE_VALUE) return;
            auto begin = static_cast<uint8_t*>(info.lpBaseOfDll), end = begin + info.SizeOfImage;
            THREADENTRY32 entry{ sizeof(entry) };
            int threads = 0;
            ULONGLONG total = 0, busiest = 0;
            for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry))
            {
                if (entry.th32OwnerProcessID != GetCurrentProcessId()) continue;
                HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
                if (!thread) continue;
                void* start = nullptr;
                FILETIME created, exited, kernel, user;
                // 9 = ThreadQuerySetWin32StartAddress
                if (query(thread, 9, &start, sizeof(start), nullptr) == 0 && start >= begin && start < end &&
                    GetThreadTimes(thread, &created, &exited, &kernel, &user))
                {
                    ULONGLONG ms = ((static_cast<ULONGLONG>(kernel.dwHighDateTime) << 32 | kernel.dwLowDateTime) +
                                    (static_cast<ULONGLONG>(user.dwHighDateTime) << 32 | user.dwLowDateTime)) / 10000;
                    ++threads;
                    total += ms;
                    busiest = (std::max)(busiest, ms);
                }
                CloseHandle(thread);
            }
            CloseHandle(snapshot);
            Log(L"pdpl: %s: %d thread(s) from pd_loader_core.dll used %llu ms of CPU (busiest %llu ms); %u lines of output, %u file opens",
                when, threads, total, busiest, g_lines.load(), g_opens.load());
        }
    }

    void BeforeLoad()
    {
        if (g_prepared.exchange(true)) return;
        // A console of pdnet's own, hidden at once: PDPL's AllocConsole then fails (one console per process), so it
        // neither shows its window nor reopens its output to it. Where no console can be made (Microsoft Store copies),
        // PDPL's fails too and the pipes below are all it has.
        bool console = AllocConsole() != FALSE;
        if (console)
            if (HWND window = GetConsoleWindow()) ShowWindow(window, SW_HIDE);
        bool pipes = CreatePipe(&g_inRead, &g_inWrite, nullptr, 0) && CreatePipe(&g_outRead, &g_outWrite, nullptr, 1u << 20);
        if (pipes)
        {
            SetStdHandle(STD_INPUT_HANDLE, g_inRead);
            SetStdHandle(STD_OUTPUT_HANDLE, g_outWrite);
            SetStdHandle(STD_ERROR_HANDLE, g_outWrite);
            std::thread(ReadOutput).detach();
        }
        Log(L"pdpl: ready for PDPL (%s, %s)", console ? L"hidden console" : L"no console", pipes ? L"its output goes to pdnet\\pdpl.log" : L"pipes NOT made - its output is lost");
    }

    void Report()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            HMODULE module = GetModuleHandleW(L"pd_loader_core.dll");
            if (!module)
            {
                Log(L"pdpl: not in the game (mods=off, safe mode, or pdlaunch couldn't load it: see pdlaunch.log)");
                return;
            }
            Log(L"pdpl: pd_loader_core.dll is in the game%s", g_prepared ? L"" : L" (without pdnet's console and pipes: an older pdlaunch?)");
            std::thread([module]
            {
                Sleep(60000);
                LogThreadTimes(module, L"after 1 minute");
                Sleep(240000);
                LogThreadTimes(module, L"after 5 minutes");
            }).detach();
        });
    }
}
