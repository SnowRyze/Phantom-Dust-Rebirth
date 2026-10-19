// pdlaunch: loads pdnet into an UNMODIFIED Phantom Dust (e.g. the Microsoft Store install, whose files can't be changed).
//
// Windows' package debug setting (IPackageDebugSettings::EnableDebugging, see store-setup.ps1) makes Windows start the
// game suspended and run "pdlaunch.exe -p <pid> -tid <main thread id>". pdlaunch then:
//   1. loads pdnet.dll (next to pdlaunch.exe) into the game with a remote LoadLibraryW thread,
//   2. runs pdnet's PdInjectedInit export there (hooks the game's WinRT activation, starts EOS),
//   3. resumes the game's main thread.
// It always resumes the game, even if a step fails (the game then runs without pdnet). Results go to pdlaunch.log.
// The folder must be readable by the game's sandbox (store-setup.ps1 grants ALL APPLICATION PACKAGES read access).
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <string>

namespace
{
    std::wstring g_dir;

    void Log(const wchar_t* fmt, ...)
    {
        FILE* f = nullptr;
        if (_wfopen_s(&f, (g_dir + L"pdlaunch.log").c_str(), L"a, ccs=UTF-8") || !f) return;
        SYSTEMTIME t;
        GetLocalTime(&t);
        fwprintf(f, L"%04u-%02u-%02u %02u:%02u:%02u.%03u ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        va_list args;
        va_start(args, fmt);
        vfwprintf(f, fmt, args);
        va_end(args);
        fwprintf(f, L"\n");
        fclose(f);
    }

    // Runs `start(arg)` in the target and waits for it. Returns false on failure or timeout.
    bool RunRemote(HANDLE process, LPTHREAD_START_ROUTINE start, void* arg, DWORD timeoutMs, const wchar_t* what)
    {
        HANDLE thread = CreateRemoteThread(process, nullptr, 0, start, arg, 0, nullptr);
        if (!thread)
        {
            Log(L"%s: CreateRemoteThread failed (%lu)", what, GetLastError());
            return false;
        }
        DWORD wait = WaitForSingleObject(thread, timeoutMs);
        CloseHandle(thread);
        if (wait != WAIT_OBJECT_0)
        {
            Log(L"%s: no answer in %lu ms", what, timeoutMs);
            return false;
        }
        return true;
    }

    HMODULE FindRemoteModule(HANDLE process, const std::wstring& path)
    {
        HMODULE modules[1024];
        DWORD needed = 0;
        if (!EnumProcessModulesEx(process, modules, sizeof(modules), &needed, LIST_MODULES_64BIT)) return nullptr;
        for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; ++i)
        {
            wchar_t name[MAX_PATH];
            if (GetModuleFileNameExW(process, modules[i], name, MAX_PATH) && _wcsicmp(name, path.c_str()) == 0) return modules[i];
        }
        return nullptr;
    }

    bool Inject(DWORD pid)
    {
        std::wstring dll = g_dir + L"pdnet.dll";
        if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            Log(L"pdnet.dll not found next to pdlaunch.exe");
            return false;
        }
        HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
        if (!process)
        {
            Log(L"OpenProcess(%lu) failed (%lu)", pid, GetLastError());
            return false;
        }

        bool ok = false;
        size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
        void* remotePath = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (remotePath && WriteProcessMemory(process, remotePath, dll.c_str(), bytes, nullptr))
        {
            // kernel32 is mapped at the same address in every process of this boot.
            auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
            if (RunRemote(process, loadLibrary, remotePath, 30000, L"LoadLibraryW(pdnet.dll)"))
            {
                HMODULE remote = FindRemoteModule(process, dll);
                // The export's offset is the same in both copies of the DLL; find it in a local copy loaded as data.
                HMODULE local = LoadLibraryExW(dll.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
                FARPROC init = local ? GetProcAddress(local, "PdInjectedInit") : nullptr;
                if (!remote)
                {
                    Log(L"pdnet.dll did not load in the game (missing read access for app packages? run store-setup.ps1 again)");
                }
                else if (!init)
                {
                    Log(L"pdnet.dll has no PdInjectedInit export (old build?)");
                }
                else
                {
                    auto remoteInit = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uint8_t*>(remote) + (reinterpret_cast<uint8_t*>(init) - reinterpret_cast<uint8_t*>(local)));
                    ok = RunRemote(process, remoteInit, nullptr, 30000, L"PdInjectedInit");
                    if (ok) Log(L"pdnet loaded into the game (pid %lu)", pid);
                }
                if (local) FreeLibrary(local);
            }
        }
        else
        {
            Log(L"could not write into the game process (%lu)", GetLastError());
        }
        if (remotePath) VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return ok;
    }
}

int wmain(int argc, wchar_t** argv)
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g_dir = exe;
    g_dir = g_dir.substr(0, g_dir.find_last_of(L'\\') + 1);

    DWORD pid = 0, tid = 0;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (_wcsicmp(argv[i], L"-p") == 0) pid = wcstoul(argv[i + 1], nullptr, 10);
        if (_wcsicmp(argv[i], L"-tid") == 0) tid = wcstoul(argv[i + 1], nullptr, 10);
    }
    if (!pid || !tid)
    {
        wprintf(L"pdlaunch is started by Windows when Phantom Dust starts (after store-setup.ps1). Run the game normally.\n");
        return 2;
    }

    Log(L"game started (pid %lu, main thread %lu)", pid, tid);
    bool injected = Inject(pid);

    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
    DWORD previous = thread ? ResumeThread(thread) : static_cast<DWORD>(-1);
    if (thread) CloseHandle(thread);
    Log(L"game resumed (suspend count was %ld)%s", static_cast<long>(previous), injected ? L"" : L" WITHOUT pdnet");
    return injected ? 0 : 1;
}
