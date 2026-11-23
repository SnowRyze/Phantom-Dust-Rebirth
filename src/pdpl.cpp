#include "pch.h"
#include "pdpl.h"
#include "log.h"
#include <algorithm>
#include <cstdio>
#include <set>
#include <share.h>
#include <thread>
#include <tlhelp32.h>
#pragma comment(lib, "user32.lib") // ShowWindow

namespace pdnet::pdpl
{
    namespace
    {
        HANDLE g_inRead = nullptr, g_inWrite = nullptr;   // PDPL's standard input: nothing is ever written
        HANDLE g_outRead = nullptr, g_outWrite = nullptr; // PDPL's standard output and error when there's no console
        HANDLE g_console = nullptr;                       // pdnet's hidden console: PDPL's standard output and error
        std::atomic<bool> g_prepared{ false };
        std::atomic<uint32_t> g_lines{ 0 }, g_opens{ 0 };

        // PDPL colours its messages with ANSI escapes ("\033[32m"): left out of the log. (A console without escape
        // handling keeps them as text, with the escape shown as U+2190, so that form goes too.)
        std::string WithoutEscapes(const std::string& s)
        {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i)
            {
                size_t skip = s[i] == '\033' ? 1 : s.compare(i, 3, "\xE2\x86\x90") == 0 ? 3 : 0;
                if (skip && i + skip < s.size() && s[i + skip] == '[')
                {
                    i += skip + 1;
                    while (i < s.size() && !(s[i] >= '@' && s[i] <= '~')) ++i; // up to the final letter
                    continue;
                }
                if (s[i] != '\r') out += s[i];
            }
            return out;
        }

        // LocalState\pdnet\pdpl.log (the last start's is kept as pdpl.prev.log).
        FILE* OpenLog()
        {
            std::wstring dir = LocalStatePath() + L"\\pdnet";
            CreateDirectoryW(dir.c_str(), nullptr);
            MoveFileExW((dir + L"\\pdpl.log").c_str(), (dir + L"\\pdpl.prev.log").c_str(), MOVEFILE_REPLACE_EXISTING);
            return _wfsopen((dir + L"\\pdpl.log").c_str(), L"wb", _SH_DENYNO);
        }

        // One line of PDPL's output: into pdpl.log with the time; its own messages (not the CreateFile2 lines, one per
        // file the game opens) also into pdnet.log, the first 200 of them.
        void Emit(FILE* file, std::string line, uint32_t& told)
        {
            line = WithoutEscapes(line);
            while (!line.empty() && line.back() == ' ') line.pop_back();
            while (line.rfind("> ", 0) == 0) line.erase(0, 2); // its command prompt, followed by the next message
            if (line.empty() || line == ">") return;
            ++g_lines;
            bool open = line.rfind("CreateFile2", 0) == 0;
            if (open) ++g_opens;
            if (file)
            {
                SYSTEMTIME t;
                GetLocalTime(&t);
                fprintf(file, "%02u:%02u:%02u.%03u %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line.c_str());
            }
            if (!open && told < 200)
            {
                ++told;
                Log(L"pdpl: %S", line.c_str());
            }
        }

        // PDPL's output through the pipe (no console could be made). Its C runtime buffers a pipe: lines come in 4 KB blocks.
        void ReadPipe()
        {
            FILE* file = OpenLog();
            std::string pending;
            char buf[8192];
            DWORD got = 0;
            uint32_t told = 0;
            while (ReadFile(g_outRead, buf, sizeof(buf), &got, nullptr) && got)
            {
                pending.append(buf, got);
                for (size_t end; (end = pending.find('\n')) != std::string::npos;)
                {
                    Emit(file, pending.substr(0, end), told);
                    pending.erase(0, end + 1);
                }
                if (file) fflush(file);
            }
            if (file) fclose(file);
            Log(L"pdpl: its output closed (%u lines, %u file opens)", g_lines.load(), g_opens.load());
        }

        // PDPL's output through pdnet's hidden console: a C runtime writes to a console at once (no buffering), so every
        // 100 ms the rows written since are read and the buffer starts again near its end. (Tested outside the game with a
        // probe that loads pd_loader_core.dll this way: every line came through, at once; through the pipe the first
        // lines waited for 4 KB, which the game never reached in a minute.)
        void ReadConsoleText()
        {
            FILE* file = OpenLog();
            uint32_t told = 0;
            SHORT lastRow = 0;
            std::wstring row;
            auto readRows = [&](SHORT from, SHORT to, SHORT width)
            {
                row.resize(width);
                for (SHORT y = from; y < to; ++y)
                {
                    DWORD got = 0;
                    if (!ReadConsoleOutputCharacterW(g_console, row.data(), width, COORD{ 0, y }, &got)) continue;
                    Emit(file, winrt::to_string(std::wstring_view(row.data(), got)), told);
                }
            };
            for (;;)
            {
                Sleep(100);
                CONSOLE_SCREEN_BUFFER_INFO info;
                if (!GetConsoleScreenBufferInfo(g_console, &info)) continue;
                SHORT cur = info.dwCursorPosition.Y, width = info.dwSize.X;
                if (cur < lastRow) lastRow = 0;
                readRows(lastRow, cur, width);
                lastRow = cur;
                if (cur > info.dwSize.Y - 300) // near the end: read anything since, then start the buffer again
                {
                    if (GetConsoleScreenBufferInfo(g_console, &info)) readRows(lastRow, info.dwCursorPosition.Y, width);
                    DWORD written = 0;
                    FillConsoleOutputCharacterW(g_console, L' ', static_cast<DWORD>(width) * info.dwSize.Y, COORD{ 0, 0 }, &written);
                    SetConsoleCursorPosition(g_console, COORD{ 0, 0 });
                    lastRow = 0;
                }
                if (file) fflush(file);
            }
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

    namespace
    {
        struct Functions
        {
            int (*mount)(const char* path, const char* point, int append) = nullptr;
            int (*unmount)(const char* path) = nullptr;
            char** (*searchPath)() = nullptr;
            void (*freeList)(void*) = nullptr;
            int (*lastError)() = nullptr;
            const char* (*errorText)(int) = nullptr;
            int (*isInit)() = nullptr;
            HANDLE* openMutex = nullptr; // PDPL's variable
            bool ok = false;
        };

        const Functions& Pdpl()
        {
            static Functions f = []
            {
                Functions r;
                HMODULE m = GetModuleHandleW(L"pd_loader_core.dll");
                if (!m) return r;
                auto get = [&](auto& to, const char* name) { to = reinterpret_cast<std::remove_reference_t<decltype(to)>>(GetProcAddress(m, name)); return to != nullptr; };
                r.ok = get(r.mount, "PHYSFS_mount") && get(r.unmount, "PHYSFS_unmount") && get(r.searchPath, "PHYSFS_getSearchPath") &&
                       get(r.freeList, "PHYSFS_freeList") && get(r.lastError, "PHYSFS_getLastErrorCode") && get(r.errorText, "PHYSFS_getErrorByCode") &&
                       get(r.isInit, "PHYSFS_isInit") && get(r.openMutex, "open_mutex");
                Log(L"pdpl: live mod loading: %s", r.ok ? L"PDPL's functions are there" : L"this PDPL doesn't export its functions - mods change at game start only");
                return r;
            }();
            return f;
        }

        // PDPL's lock, held by its hook while it opens a file for the game.
        struct OpenLock
        {
            bool held = false;
            OpenLock()
            {
                HANDLE h = Pdpl().openMutex ? *Pdpl().openMutex : nullptr;
                DWORD wait = h ? WaitForSingleObject(h, 5000) : WAIT_FAILED;
                held = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
            }
            ~OpenLock() { if (held) ReleaseMutex(*Pdpl().openMutex); }
        };

        std::vector<std::string> SearchPath()
        {
            std::vector<std::string> out;
            if (char** list = Pdpl().searchPath())
            {
                for (char** p = list; *p; ++p) out.push_back(*p);
                Pdpl().freeList(list);
            }
            return out;
        }

        bool IsArchive(const std::string& path)
        {
            auto ends = [&](const char* e) { size_t n = strlen(e); return path.size() > n && _stricmp(path.c_str() + path.size() - n, e) == 0; };
            return ends(".zip") || ends(".7z");
        }

        const char* LastError() { const char* t = Pdpl().errorText(Pdpl().lastError()); return t ? t : "?"; }
    }

    bool LiveReady()
    {
        const Functions& f = Pdpl();
        return f.ok && f.isInit() && *f.openMutex;
    }

    bool UnmountArchives(std::string& why)
    {
        if (!LiveReady()) { why = "PDPL's functions aren't there"; return false; }
        OpenLock lock;
        if (!lock.held) { why = "PDPL is busy opening a file"; return false; }
        int taken = 0;
        for (const std::string& path : SearchPath())
        {
            if (!IsArchive(path)) continue; // the mods folder and the game folder
            if (Pdpl().unmount(path.c_str())) { ++taken; continue; }
            why = path + ": " + LastError();
            return false;
        }
        Log(L"pdpl: live: %d archive(s) unmounted", taken);
        return true;
    }

    int MountArchives()
    {
        if (!LiveReady()) return 0;
        OpenLock lock;
        std::vector<std::string> path = SearchPath();
        std::string mods, game;
        std::set<std::string> mounted;
        for (const std::string& p : path)
        {
            if (IsArchive(p)) mounted.insert(p);
            else if (mods.empty()) mods = p; // PDPL mounts its mods folder first and the game folder last
            else game = p;
        }
        if (!lock.held || mods.empty())
        {
            Log(L"pdpl: live: archives not mounted (%s)", lock.held ? L"PDPL's mods folder isn't in its list" : L"PDPL is busy opening a file");
            return 0;
        }
        std::vector<std::string> files; // the names, sorted as PhysicsFS sorts them for PDPL (by bytes)
        WIN32_FIND_DATAW found;
        HANDLE find = FindFirstFileW((std::wstring(winrt::to_hstring(mods)) + L"\\*").c_str(), &found);
        if (find != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::string name = winrt::to_string(found.cFileName);
                if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsArchive(name)) files.push_back(name);
            } while (FindNextFileW(find, &found));
            FindClose(find);
        }
        std::sort(files.begin(), files.end());
        // The game folder stays last: out, the archives in, back in. (Files it would answer are opened normally meanwhile.)
        if (!game.empty()) Pdpl().unmount(game.c_str());
        int count = 0;
        for (const std::string& name : files)
        {
            std::string full = mods + "\\" + name;
            if (mounted.count(full)) Pdpl().unmount(full.c_str()); // left from an unmount that failed half way: in its place
            if (Pdpl().mount(full.c_str(), "/", 1)) ++count;
            else Log(L"pdpl: live: %S not mounted: %S", name.c_str(), LastError());
        }
        if (!game.empty() && !Pdpl().mount(game.c_str(), "/", 1)) Log(L"pdpl: live: the game folder not mounted: %S", LastError());
        Log(L"pdpl: live: %d of %zu archive(s) mounted", count, files.size());
        return count;
    }

    void BeforeLoad()
    {
        if (g_prepared.exchange(true)) return;
        CreateDirectoryW((LocalStatePath() + L"\\pdnet").c_str(), nullptr);
        if (FILE* f = nullptr; !_wfopen_s(&f, (LocalStatePath() + L"\\pdnet\\mods-boot.txt").c_str(), L"w") && f)
        {
            fputs("PDPL loaded; deleted once the title screen is passed. Found at a game start: that start goes without mods.\n", f);
            fclose(f);
        }
        // A console of pdnet's own, hidden at once: PDPL's AllocConsole then fails (one console per process), so it
        // neither shows its window nor reopens its output to it. That console is PDPL's standard output, read back into
        // pdpl.log. Where no console can be made (Microsoft Store copies), PDPL's fails too and a pipe is its output.
        // Its standard input is an empty pipe either way: its command loop waits there instead of spinning.
        if (AllocConsole())
        {
            if (HWND window = GetConsoleWindow()) ShowWindow(window, SW_HIDE);
            g_console = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (g_console == INVALID_HANDLE_VALUE) g_console = nullptr;
        }
        if (g_console)
        {
            SetConsoleScreenBufferSize(g_console, COORD{ 400, 9000 });
            DWORD mode = 0;
            if (GetConsoleMode(g_console, &mode)) SetConsoleMode(g_console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING); // colours applied, not kept as text
        }
        bool input = CreatePipe(&g_inRead, &g_inWrite, nullptr, 0) != FALSE;
        if (input) SetStdHandle(STD_INPUT_HANDLE, g_inRead);
        bool output = false;
        if (g_console)
        {
            SetStdHandle(STD_OUTPUT_HANDLE, g_console);
            SetStdHandle(STD_ERROR_HANDLE, g_console);
            std::thread(ReadConsoleText).detach();
            output = true;
        }
        else if (CreatePipe(&g_outRead, &g_outWrite, nullptr, 1u << 20))
        {
            SetStdHandle(STD_OUTPUT_HANDLE, g_outWrite);
            SetStdHandle(STD_ERROR_HANDLE, g_outWrite);
            std::thread(ReadPipe).detach();
            output = true;
        }
        Log(L"pdpl: ready for PDPL (%s; input %s)", !output ? L"its output is lost" : g_console ? L"hidden console, read into pdnet\\pdpl.log" : L"no console: a pipe into pdnet\\pdpl.log, in 4 KB blocks",
            input ? L"an empty pipe" : L"NOT replaced");
    }

    void TitleReached()
    {
        if (DeleteFileW((LocalStatePath() + L"\\pdnet\\mods-boot.txt").c_str())) Log(L"pdpl: title screen passed - mods-boot.txt deleted");
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
            Pdpl(); // logs whether mods can change while the game runs
            Log(L"pdpl: pd_loader_core.dll is in the game%s", g_prepared ? L"" : L" (without pdnet's console and pipes: an older pdlaunch?)");
            // The mod list pdlaunch put in PDPL's mods folder this start (launcher\modsync.cpp).
            if (FILE* f = nullptr; !_wfopen_s(&f, (LocalStatePath() + L"\\pdnet\\mods-state.txt").c_str(), L"rb") && f)
            {
                char line[2048];
                int n = 0;
                while (fgets(line, sizeof(line), f))
                {
                    if (strncmp(line, "mod\t", 4) != 0) continue;
                    // mod id on|off source name version ...
                    char id[128] = "", on[8] = "", source[16] = "", name[256] = "";
                    sscanf_s(line + 4, "%127[^\t]\t%7[^\t]\t%15[^\t]\t%255[^\t]", id, static_cast<unsigned>(sizeof(id)), on, static_cast<unsigned>(sizeof(on)),
                             source, static_cast<unsigned>(sizeof(source)), name, static_cast<unsigned>(sizeof(name)));
                    Log(L"pdpl: mod %d: %S (%S, %S) %S", ++n, id, source, on, name);
                }
                fclose(f);
                if (!n) Log(L"pdpl: no mods (mods-state.txt lists none)");
            }
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
