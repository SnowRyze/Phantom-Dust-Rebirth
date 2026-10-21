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
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <algorithm>
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

namespace
{
    // pdnet.ini (next to pdlaunch.exe) value, or `fallback`.
    std::wstring Setting(const wchar_t* key, const wchar_t* fallback)
    {
        FILE* f = nullptr;
        std::wstring value = fallback;
        if (_wfopen_s(&f, (g_dir + L"pdnet.ini").c_str(), L"r, ccs=UTF-8") || !f) return value;
        wchar_t line[512];
        size_t keyLength = wcslen(key);
        while (fgetws(line, 512, f))
        {
            std::wstring s = line;
            while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' ')) s.pop_back();
            if (s.compare(0, keyLength, key) == 0 && s.size() > keyLength && s[keyLength] == L'=') value = s.substr(keyLength + 1);
        }
        fclose(f);
        return value;
    }

    // The game's audio session (the Volume Mixer entry) on the default playback device, or null.
    ISimpleAudioVolume* FindGameVolume(DWORD pid)
    {
        IMMDeviceEnumerator* devices = nullptr;
        IMMDevice* device = nullptr;
        IAudioSessionManager2* manager = nullptr;
        IAudioSessionEnumerator* sessions = nullptr;
        ISimpleAudioVolume* found = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&devices))) &&
            SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
            SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&manager))) &&
            SUCCEEDED(manager->GetSessionEnumerator(&sessions)))
        {
            int count = 0;
            sessions->GetCount(&count);
            for (int i = 0; i < count && !found; ++i)
            {
                IAudioSessionControl* control = nullptr;
                IAudioSessionControl2* control2 = nullptr;
                DWORD owner = 0;
                if (SUCCEEDED(sessions->GetSession(i, &control)) &&
                    SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&control2))) &&
                    SUCCEEDED(control2->GetProcessId(&owner)) && owner == pid)
                {
                    control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&found));
                }
                if (control2) control2->Release();
                if (control) control->Release();
            }
        }
        if (sessions) sessions->Release();
        if (manager) manager->Release();
        if (device) device->Release();
        if (devices) devices->Release();
        return found;
    }

    // Windows resets the game's Volume Mixer level to 100% every start, and the game only applies its own sound
    // settings after the splash screen, so the logos play at full volume. While the game runs: once its audio
    // session appears, hold it at splash_volume% of the remembered level for splash_seconds, then the remembered
    // level; after that, remember every change the player makes (volume.txt next to pdlaunch.exe).
    //   pdnet.ini: remember_volume=on|off, splash_volume=40 (percent), splash_seconds=12
    void WatchVolume(DWORD pid)
    {
        bool remember = _wcsicmp(Setting(L"remember_volume", L"on").c_str(), L"off") != 0;
        float splash = std::clamp(static_cast<float>(_wtof(Setting(L"splash_volume", L"40").c_str())) / 100.0f, 0.0f, 1.0f);
        DWORD splashMs = static_cast<DWORD>(std::clamp(_wtoi(Setting(L"splash_seconds", L"12").c_str()), 0, 120)) * 1000;
        std::wstring file = g_dir + L"volume.txt";
        float saved = -1;
        if (FILE* f = nullptr; remember && !_wfopen_s(&f, file.c_str(), L"r") && f)
        {
            fwscanf_s(f, L"%f", &saved);
            fclose(f);
        }
        if (!remember && splash >= 1.0f) return;

        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!process) return;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ISimpleAudioVolume* volume = nullptr;
        DWORD found = 0;
        float level = 1.0f, last = -1;
        while (WaitForSingleObject(process, volume ? 250 : 50) == WAIT_TIMEOUT)
        {
            if (!volume)
            {
                volume = FindGameVolume(pid);
                if (!volume) continue;
                found = GetTickCount();
                volume->GetMasterVolume(&level);
                if (remember && saved >= 0 && saved <= 1) level = saved;
                Log(L"audio: game session found; volume %d%% (splash %d%% of it for %lu s)", static_cast<int>(level * 100 + 0.5f), static_cast<int>(splash * 100 + 0.5f), splashMs / 1000);
            }
            DWORD age = GetTickCount() - found;
            if (age < splashMs)
            {
                volume->SetMasterVolume(level * splash, nullptr);
            }
            else if (age < splashMs + 3000)
            {
                volume->SetMasterVolume(level, nullptr); // held a moment, in case the game sets it as it starts up
                last = level;
            }
            else if (remember)
            {
                float now = 0;
                if (SUCCEEDED(volume->GetMasterVolume(&now)) && (last < 0 || now < last - 0.004f || now > last + 0.004f))
                {
                    last = now;
                    FILE* f = nullptr;
                    if (!_wfopen_s(&f, file.c_str(), L"w") && f)
                    {
                        fwprintf(f, L"%.3f\n", now);
                        fclose(f);
                    }
                    Log(L"audio: remembered volume %d%%", static_cast<int>(now * 100 + 0.5f));
                }
            }
        }
        if (volume) volume->Release();
        CloseHandle(process);
        CoUninitialize();
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
    WatchVolume(pid); // until the game exits
    return injected ? 0 : 1;
}
