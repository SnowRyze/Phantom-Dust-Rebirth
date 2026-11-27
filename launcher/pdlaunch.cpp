// pdlaunch: loads pdnet into an UNMODIFIED Phantom Dust (e.g. the Microsoft Store install, whose files can't be changed).
//
// Windows' package debug setting (IPackageDebugSettings::EnableDebugging, see store-setup.ps1) makes Windows start the
// game suspended and run "pdlaunch.exe -p <pid> -tid <main thread id>". pdlaunch then:
//   1. loads pdnet.dll (next to pdlaunch.exe) into the game with a remote LoadLibraryW thread,
//   2. runs pdnet's PdInjectedInit export there (hooks the game's WinRT activation, starts EOS),
//   3. loads PDPL (Torphedo's mod loader, pd_loader_core.dll) unless mods=off (InjectPdpl; Conceptual),
//   4. resumes the game's main thread.
// It always resumes the game, even if a step fails (the game then runs without pdnet). Results go to pdlaunch.log.
// The folder must be readable by the game's sandbox (store-setup.ps1 grants ALL APPLICATION PACKAGES read access).
#include <windows.h>
#include <psapi.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <appmodel.h>
#include <winhttp.h>
#include <wincred.h>
#include <shobjidl.h>
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <share.h>
#include <thread>
#include <vector>
#include <SDL3/SDL.h>  // types only: SDL3.dll is loaded at run time (ReadControllers)
#include "padshare.h"
#include "logscrub.h"
#include <cstdarg>
#include "modsync.h"
#include <bcrypt.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
    std::wstring g_dir;

    // The player's in-game name, which the log shows in place of the PC's own names (logscrub.h; the user, 2026-11-22):
    // display_name from the kit's pdnet.ini, then the game's settings.ini.
    const std::wstring& LogName()
    {
        static std::wstring name = []
        {
            wchar_t local[MAX_PATH] = {};
            DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
            std::vector<std::wstring> files{ g_dir + L"pdnet.ini" };
            if (n && n < MAX_PATH) files.push_back(std::wstring(local) + L"\\Packages\\Microsoft.MSEsper_8wekyb3d8bbwe\\LocalState\\pdnet\\settings.ini");
            return pdnet::logscrub::ReadName(files);
        }();
        return name;
    }

    void Log(const wchar_t* fmt, ...)
    {
        FILE* f = nullptr;
        if (_wfopen_s(&f, (g_dir + L"pdlaunch.log").c_str(), L"a, ccs=UTF-8") || !f) return;
        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t body[4096];
        va_list args;
        va_start(args, fmt);
        _vsnwprintf_s(body, _TRUNCATE, fmt, args);
        va_end(args);
        std::wstring text = body;
        static const std::wstring computer = pdnet::logscrub::ComputerName();
        pdnet::logscrub::Scrub(text, LogName(), computer);
        fwprintf(f, L"%04u-%02u-%02u %02u:%02u:%02u.%03u %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, text.c_str());
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

    // Loads `dll` in the game with a remote LoadLibraryW thread; its module handle there, or null (logged).
    HMODULE RemoteLoadLibrary(HANDLE process, const std::wstring& dll, const wchar_t* what)
    {
        HMODULE remote = nullptr;
        size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
        void* remotePath = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (remotePath && WriteProcessMemory(process, remotePath, dll.c_str(), bytes, nullptr))
        {
            // kernel32 is mapped at the same address in every process of this boot.
            auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
            std::wstring label = std::wstring(L"LoadLibraryW(") + what + L")";
            if (RunRemote(process, loadLibrary, remotePath, 30000, label.c_str()))
            {
                remote = FindRemoteModule(process, dll);
                if (!remote) Log(L"%s did not load in the game (missing read access for app packages? run Setup.bat again)", what);
            }
        }
        else
        {
            Log(L"could not write into the game process (%lu)", GetLastError());
        }
        if (remotePath) VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        return remote;
    }

    // Runs the export `name` of `dll` (loaded in the game at `remote`) on a remote thread and waits for it.
    bool RunRemoteExport(HANDLE process, HMODULE remote, const std::wstring& dll, const char* name, DWORD timeoutMs)
    {
        // The export's offset is the same in both copies of the DLL; find it in a local copy loaded as data.
        HMODULE local = LoadLibraryExW(dll.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
        FARPROC function = local ? GetProcAddress(local, name) : nullptr;
        bool ok = false;
        std::wstring label(name, name + strlen(name));
        if (!function)
        {
            Log(L"%s has no %s export (old build?)", dll.substr(dll.find_last_of(L'\\') + 1).c_str(), label.c_str());
        }
        else
        {
            auto start = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uint8_t*>(remote) + (reinterpret_cast<uint8_t*>(function) - reinterpret_cast<uint8_t*>(local)));
            ok = RunRemote(process, start, nullptr, timeoutMs, label.c_str());
        }
        if (local) FreeLibrary(local);
        return ok;
    }

    HANDLE OpenGame(DWORD pid)
    {
        HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
        if (!process) Log(L"OpenProcess(%lu) failed (%lu)", pid, GetLastError());
        return process;
    }

    bool Inject(DWORD pid)
    {
        std::wstring dll = g_dir + L"pdnet.dll";
        if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            Log(L"pdnet.dll not found next to pdlaunch.exe");
            return false;
        }
        HANDLE process = OpenGame(pid);
        if (!process) return false;
        bool ok = false;
        if (HMODULE remote = RemoteLoadLibrary(process, dll, L"pdnet.dll"))
        {
            ok = RunRemoteExport(process, remote, dll, "PdInjectedInit", 30000);
            if (ok) Log(L"pdnet loaded into the game (pid %lu)", pid);
        }
        CloseHandle(process);
        return ok;
    }

    std::wstring Setting(const wchar_t* key, const wchar_t* fallback); // (below)
    bool UrlGet(const std::wstring& url, std::string& body, unsigned long timeoutMs); // (below)
    bool LatestReleaseTag(const std::wstring& repo, std::string& tag, unsigned long timeoutMs); // (below)
    std::string Sha256(const std::string& data); // (below)
    // The mod catalog (mod_list_url= in pdnet.ini or settings.ini): mods.txt on this repository's own mod-catalog branch,
    // which holds only that file and is written only by the Mod Admin app (outside the repository, like the leaderboard
    // admin tool), so every kit (Conceptual, Experimental, Stable) reads the same whitelist.
    constexpr wchar_t kModListUrl[] = L"https://raw.githubusercontent.com/SnowRyze/Phantom-Dust-Rebirth/mod-catalog/mods.txt";
    const modsync::Net kModNet{ UrlGet, Sha256, LatestReleaseTag };

    // Mod loader (Conceptual, step 1; docs\MOD_LOADER_CODE.md): PDPL, Torphedo's Phantom Dust Plugin Loader
    // (pd_loader_core.dll, next to pdlaunch.exe), loaded after pdnet while the game is still suspended. It loads mod files
    // from RoamingState\mods. Skipped with mods=off, and after a start that never reached the title screen (pdnet keeps
    // LocalState\pdnet\mods-boot.txt until then). First pdnet's PdBeforePdpl gives it a hidden console and pipes for its
    // input and output (pdpl.h). PDPL's DllMain only starts a thread, which hooks CreateFile2 and then holds the game's file
    // opens until its mods are mounted; the game mustn't run before that hook is in, or its first files would bypass PDPL.
    // MinHook puts a jump (E9) at the start of KERNELBASE!CreateFile2, which is at the same address in every process of
    // this boot, so pdlaunch reads that byte in the game until it changes (5 s at most).
    std::wstring GameFolder(DWORD pid) // where PDUWP.exe runs from (new map slots read the game's stage list there)
    {
        std::wstring dir;
        if (HANDLE game = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
        {
            wchar_t image[MAX_PATH] = {};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(game, 0, image, &size)) { dir = image; dir = dir.substr(0, dir.find_last_of(L'\\')); }
            CloseHandle(game);
        }
        return dir;
    }

    // Live mod loading (pdnet's mods::ApplyLive): until the game exits, mods are synced again whenever pdnet asks. Files in
    // LocalState\pdnet, each "<number>[<tab>text]" and written whole (a .tmp renamed):
    //   mods-live.txt   pdnet:    a new request
    //   mods-reply.txt  pdlaunch: "<n> working" at once; "<n> ready" once everything is downloaded and the changes can be made
    //                             while the game runs; then "<n> done", or "<n> restart <why>" / "<n> failed" (nothing changed)
    //   mods-go.txt     pdnet:    "<n> go" when its archives are unmounted (or "<n> no": not now)
    void WriteLive(const std::wstring& path, const std::string& text)
    {
        std::wstring tmp = path + L".tmp";
        FILE* f = nullptr;
        if (_wfopen_s(&f, tmp.c_str(), L"wb") || !f) return;
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
        // The game reads the reply 20 times a second, and a file that's open can't be replaced: a lost reply left it waiting
        // 5 minutes (2026-11-23), so it's tried until it works.
        for (int i = 0; i < 400 && !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING); ++i) Sleep(5);
    }

    std::string ReadLive(const std::wstring& path)
    {
        char buf[512];
        FILE* f = _wfsopen(path.c_str(), L"rb", _SH_DENYNO);
        if (!f) return {};
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        return std::string(buf, n);
    }

    void WatchLiveMods(DWORD pid, std::wstring localState)
    {
        std::wstring dir = localState + L"\\pdnet\\", request = dir + L"mods-live.txt", reply = dir + L"mods-reply.txt", go = dir + L"mods-go.txt";
        HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!game) return;
        while (WaitForSingleObject(game, 250) == WAIT_TIMEOUT)
        {
            std::string number = ReadLive(request);
            if (number.empty() || number.find_first_not_of("0123456789") != std::string::npos) continue;
            DeleteFileW(request.c_str());
            DeleteFileW(go.c_str());
            WriteLive(reply, number + "\tworking");
            Log(L"mods: live: the game asked for its mods again (request %S)", number.c_str());
            std::string needs;
            bool refused = false;
            bool ok = modsync::Sync(localState, Log, &kModNet, Setting(L"mod_list_url", kModListUrl), GameFolder(pid), [&](const std::string& needsRestart)
            {
                needs = needsRestart;
                if (!needs.empty()) return false;
                WriteLive(reply, number + "\tready");
                for (int i = 0; i < 100; ++i) // 10 s for the game to unmount its archives
                {
                    std::string answer = ReadLive(go);
                    if (answer == number + "\tgo") return true;
                    if (answer == number + "\tno" || WaitForSingleObject(game, 100) != WAIT_TIMEOUT) break;
                }
                refused = true;
                return false;
            });
            DeleteFileW(go.c_str());
            WriteLive(reply, number + (ok ? "\tdone" : !needs.empty() ? "\trestart\t" + needs.substr(0, 300) : "\tfailed"));
            Log(L"mods: live: %s%S", ok ? L"done" : !needs.empty() ? L"not now, these load at game start: " : refused ? L"the game wasn't ready - nothing changed" : L"failed", needs.c_str());
        }
        CloseHandle(game);
    }

    void InjectPdpl(DWORD pid, const std::wstring& localState)
    {
        if (_wcsicmp(Setting(L"mods", L"on").c_str(), L"off") == 0)
        {
            Log(L"mods: off (mods=off) - PDPL not loaded");
            return;
        }
        if (!localState.empty() && GetFileAttributesW((localState + L"\\pdnet\\mods-boot.txt").c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            Log(L"mods: the last start didn't reach the title screen - starting without mods (PDPL not loaded)");
            return;
        }
        std::wstring gameDir = GameFolder(pid);
        for (const wchar_t* old : { L"mods-live.txt", L"mods-reply.txt", L"mods-go.txt" }) DeleteFileW((localState + L"\\pdnet\\" + old).c_str()); // from the last start
        modsync::Sync(localState, Log, &kModNet, Setting(L"mod_list_url", kModListUrl), gameDir); // catalog downloads, then PDPL's mods folder = the mod list, before PDPL reads it
        std::wstring dll = g_dir + L"pd_loader_core.dll", pdnet = g_dir + L"pdnet.dll";
        if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            Log(L"mods: pd_loader_core.dll not found next to pdlaunch.exe - PDPL not loaded");
            return;
        }
        HANDLE process = OpenGame(pid);
        if (!process) return;
        if (HMODULE remote = FindRemoteModule(process, pdnet))
        {
            if (!RunRemoteExport(process, remote, pdnet, "PdBeforePdpl", 10000)) Log(L"mods: PDPL may show a console window (pdnet's PdBeforePdpl didn't run)");
        }
        ULONGLONG start = GetTickCount64();
        if (RemoteLoadLibrary(process, dll, L"pd_loader_core.dll"))
        {
            auto createFile2 = reinterpret_cast<const uint8_t*>(GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "CreateFile2"));
            bool hooked = false;
            while (createFile2 && !hooked && GetTickCount64() - start < 5000)
            {
                uint8_t first = 0;
                SIZE_T read = 0;
                hooked = ReadProcessMemory(process, createFile2, &first, 1, &read) && read == 1 && first == 0xE9;
                if (!hooked) Sleep(1);
            }
            if (hooked) Log(L"mods: PDPL loaded (its file hook was in after %llu ms)", GetTickCount64() - start);
            else Log(L"mods: PDPL loaded, but its file hook wasn't seen in 5 s - resuming the game anyway");
        }
        CloseHandle(process);
    }
}
namespace
{
    // pdnet.ini (next to pdlaunch.exe) value, or `fallback`.
    // key=value from pdnet.ini next to pdlaunch, then from the game's LocalState\pdnet\settings.ini (what the in-game
    // Settings menu saved; it wins).
    std::wstring Setting(const wchar_t* key, const wchar_t* fallback)
    {
        std::wstring value = fallback;
        wchar_t local[MAX_PATH] = {};
        GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
        const std::wstring files[] = { g_dir + L"pdnet.ini", std::wstring(local) + L"\\Packages\\Microsoft.MSEsper_8wekyb3d8bbwe\\LocalState\\pdnet\\settings.ini" };
        size_t keyLength = wcslen(key);
        for (auto& file : files)
        {
            FILE* f = nullptr;
            if (_wfopen_s(&f, file.c_str(), L"r, ccs=UTF-8") || !f) continue;
            wchar_t line[512];
            while (fgetws(line, 512, f))
            {
                // By hand, "key = value" and indented lines count too. An empty value leaves the value as it was.
                constexpr const wchar_t* kBlanks = L" \t\r\n\xfeff";
                std::wstring s = line;
                size_t eq = s.find(L'='), start = s.find_first_not_of(kBlanks);
                if (eq == std::wstring::npos || start == std::wstring::npos || start >= eq) continue;
                size_t keyEnd = s.find_last_not_of(kBlanks, eq - 1) + 1;
                if (keyEnd - start != keyLength || s.compare(start, keyLength, key) != 0) continue;
                size_t first = s.find_first_not_of(kBlanks, eq + 1);
                if (first != std::wstring::npos) value = s.substr(first, s.find_last_not_of(kBlanks) + 1 - first);
            }
            fclose(f);
        }
        return value;
    }

    // One of the game's audio sessions (Volume Mixer entries) on the default playback device.
    struct GameSession
    {
        std::wstring id; // session instance id: a new one means the game opened a new session
        ISimpleAudioVolume* volume;
    };

    // All of the game's audio sessions on the default playback device (each volume to be released).
    std::vector<GameSession> FindGameVolumes(DWORD pid)
    {
        IMMDeviceEnumerator* devices = nullptr;
        IMMDevice* device = nullptr;
        IAudioSessionManager2* manager = nullptr;
        IAudioSessionEnumerator* sessions = nullptr;
        std::vector<GameSession> found;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&devices))) &&
            SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
            SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&manager))) &&
            SUCCEEDED(manager->GetSessionEnumerator(&sessions)))
        {
            int count = 0;
            sessions->GetCount(&count);
            for (int i = 0; i < count; ++i)
            {
                IAudioSessionControl* control = nullptr;
                IAudioSessionControl2* control2 = nullptr;
                DWORD owner = 0;
                if (SUCCEEDED(sessions->GetSession(i, &control)) &&
                    SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&control2))) &&
                    SUCCEEDED(control2->GetProcessId(&owner)) && owner == pid)
                {
                    GameSession s{ {}, nullptr };
                    if (wchar_t* id = nullptr; SUCCEEDED(control2->GetSessionInstanceIdentifier(&id)) && id)
                    {
                        s.id = id;
                        CoTaskMemFree(id);
                    }
                    if (SUCCEEDED(control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&s.volume)))) found.push_back(s);
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

    // Windows never remembers the game's Volume Mixer level (it starts at 100% every time), and the game only applies
    // its own sound settings after the splash screen, so the logos play at full volume. While the game runs: once its
    // audio session appears, hold it at splash_volume% of the remembered level while the title screen or intro is on
    // (pdnet keeps LocalState\pdnet\past-title.txt while any other menu is, deleted here as the game starts; until it
    // first appears at most kNoSignalMs, and not at all without pdnet), else the remembered level, remembering every
    // change the player makes there. Backing out to the title makes it quiet again. The sessions are looked up again on
    // every check: the game can open a new one later (at 100%), which then gets the current level too.
    // The level is kept in LocalState\pdnet\volume.txt, shared by every kit (older kits kept volume.txt next to pdlaunch.exe).
    //   pdnet.ini: splash_volume=40 (percent; the level is always remembered)
    constexpr DWORD kNoSignalMs = 180000;

    std::wstring PastTitleFile(const std::wstring& localState) { return localState + L"\\pdnet\\past-title.txt"; }

    void WatchVolume(DWORD pid, const std::wstring& localState, bool injected)
    {
        float splash = std::clamp(static_cast<float>(_wtof(Setting(L"splash_volume", L"40").c_str())) / 100.0f, 0.0f, 1.0f);
        std::wstring file = localState.empty() ? g_dir + L"volume.txt" : localState + L"\\pdnet\\volume.txt";
        std::wstring pastTitle = localState.empty() ? std::wstring() : PastTitleFile(localState);
        float saved = -1;
        for (const std::wstring& path : { file, g_dir + L"volume.txt" })
            if (FILE* f = nullptr; saved < 0 && !_wfopen_s(&f, path.c_str(), L"r") && f)
            {
                if (fwscanf_s(f, L"%f", &saved) != 1 || saved > 1) saved = -1;
                fclose(f);
            }

        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!process) return;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        DWORD found = 0, switched = 0; // switched: when the level last changed between quiet and normal
        float last = 1.0f;
        bool quiet = true, everPast = false;
        std::set<std::wstring> known;
        while (WaitForSingleObject(process, found ? 250 : 50) == WAIT_TIMEOUT)
        {
            std::vector<GameSession> sessions = FindGameVolumes(pid);
            if (sessions.empty()) continue;
            if (!found)
            {
                found = switched = GetTickCount();
                sessions[0].volume->GetMasterVolume(&last);
                if (saved >= 0) last = saved;
                Log(L"audio: game session found; volume %d%% (start-up %d%% of it on the title screen)", static_cast<int>(last * 100 + 0.5f), static_cast<int>(splash * 100 + 0.5f));
            }
            bool past = !pastTitle.empty() && GetFileAttributesW(pastTitle.c_str()) != INVALID_FILE_ATTRIBUTES;
            everPast = everPast || past;
            bool nowQuiet = injected && !past && (everPast || GetTickCount() - found < kNoSignalMs);
            if (nowQuiet != quiet)
            {
                quiet = nowQuiet;
                switched = GetTickCount();
                Log(L"audio: %s", quiet ? L"back on the title screen: start-up volume" : past ? L"past the title screen: normal volume" :
                                      !injected ? L"no pdnet in the game: normal volume" : L"no word from pdnet in 3 minutes: normal volume");
            }
            if (quiet || GetTickCount() - switched < 3000)
            {
                // the start-up level, or the normal level held a moment after it, in case the game sets it as it starts up
                for (auto& s : sessions)
                {
                    s.volume->SetMasterVolume(quiet ? last * splash : last, nullptr);
                    known.insert(s.id);
                }
            }
            else
            {
                float changed = -1;
                for (auto& s : sessions)
                {
                    float now = 0;
                    if (known.insert(s.id).second)
                    {
                        s.volume->SetMasterVolume(last, nullptr);
                        Log(L"audio: the game opened another audio session; set it to %d%%", static_cast<int>(last * 100 + 0.5f));
                    }
                    else if (changed < 0 && SUCCEEDED(s.volume->GetMasterVolume(&now)) && (now < last - 0.004f || now > last + 0.004f))
                        changed = now;
                }
                if (changed >= 0)
                {
                    last = changed;
                    for (auto& s : sessions) s.volume->SetMasterVolume(last, nullptr); // one level for all of the game's sessions
                    if (!localState.empty()) CreateDirectoryW((localState + L"\\pdnet").c_str(), nullptr);
                    FILE* f = nullptr;
                    if (!_wfopen_s(&f, file.c_str(), L"w") && f)
                    {
                        fwprintf(f, L"%.3f\n", last);
                        fclose(f);
                    }
                    Log(L"audio: remembered volume %d%%", static_cast<int>(last * 100 + 0.5f));
                }
            }
            for (auto& s : sessions) s.volume->Release();
        }
        CloseHandle(process);
        CoUninitialize();
    }

    // The player's identity outlives the game: uninstalling Phantom Dust deletes LocalState, where pdnet keeps the EOS
    // device credential (eos\credentials\*.cred: which Epic product user this PC signs in as, and so its scoreboard
    // entries) and pdnet\identity.txt (the local player id). A new one after a reinstall would be a second player on the
    // boards. So pdlaunch keeps a copy of each in Windows Credential Manager (generic credentials "pdnet/identity/<file>",
    // this Windows user, this PC; the files as they are, already DPAPI-protected by pdnet), and the first copy kept is
    // the player's for good: before the game starts, a missing or different file is put back from it.
    constexpr const wchar_t* kIdentityPrefix = L"pdnet/identity/";

    std::vector<std::wstring> IdentityFiles(const std::wstring& localState)
    {
        std::vector<std::wstring> files; // paths under LocalState
        WIN32_FIND_DATAW found;
        HANDLE find = FindFirstFileW((localState + L"\\eos\\credentials\\*.cred").c_str(), &found);
        if (find != INVALID_HANDLE_VALUE)
        {
            do if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(std::wstring(L"eos\\credentials\\") + found.cFileName);
            while (FindNextFileW(find, &found));
            FindClose(find);
        }
        if (GetFileAttributesW((localState + L"\\pdnet\\identity.txt").c_str()) != INVALID_FILE_ATTRIBUTES) files.push_back(L"pdnet\\identity.txt");
        return files;
    }

    bool ReadWhole(const std::wstring& path, std::vector<uint8_t>& out)
    {
        HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        DWORD size = GetFileSize(f, nullptr), got = 0;
        out.resize(size);
        bool ok = size <= CRED_MAX_CREDENTIAL_BLOB_SIZE && ReadFile(f, out.data(), size, &got, nullptr) && got == size;
        CloseHandle(f);
        return ok;
    }

    // restore: put kept files back first (before the game runs); then keep every file that has no copy yet.
    void KeepIdentity(const std::wstring& localState, bool restore)
    {
        if (localState.empty()) return;
        int restored = 0, kept = 0;
        if (restore)
        {
            DWORD count = 0;
            PCREDENTIALW* creds = nullptr;
            if (CredEnumerateW((std::wstring(kIdentityPrefix) + L"*").c_str(), 0, &count, &creds))
            {
                for (DWORD i = 0; i < count; ++i)
                {
                    std::wstring file = creds[i]->TargetName + wcslen(kIdentityPrefix);
                    if (file.find(L"..") != std::wstring::npos) continue;
                    std::wstring path = localState + L"\\" + file;
                    std::vector<uint8_t> now, kept(creds[i]->CredentialBlob, creds[i]->CredentialBlob + creds[i]->CredentialBlobSize);
                    if (ReadWhole(path, now) && now == kept) continue;
                    CreateDirectoryW((localState + L"\\eos").c_str(), nullptr);
                    CreateDirectoryW((localState + L"\\eos\\credentials").c_str(), nullptr);
                    CreateDirectoryW((localState + L"\\pdnet").c_str(), nullptr);
                    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    DWORD wrote = 0;
                    bool ok = f != INVALID_HANDLE_VALUE && WriteFile(f, kept.data(), static_cast<DWORD>(kept.size()), &wrote, nullptr) && wrote == kept.size();
                    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
                    Log(L"identity: %s %s from Windows Credential Manager%s", file.c_str(), now.empty() ? L"put back" : L"replaced (a different one had appeared)", ok ? L"" : L" - FAILED");
                    if (ok) ++restored;
                }
                CredFree(creds);
            }
        }
        for (const std::wstring& file : IdentityFiles(localState))
        {
            std::wstring target = kIdentityPrefix + file;
            PCREDENTIALW existing = nullptr;
            if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &existing))
            {
                CredFree(existing); // the first one kept stays the player's
                continue;
            }
            std::vector<uint8_t> data;
            if (!ReadWhole(localState + L"\\" + file, data) || data.empty()) continue;
            CREDENTIALW c{};
            c.Type = CRED_TYPE_GENERIC;
            c.TargetName = const_cast<LPWSTR>(target.c_str());
            c.Comment = const_cast<LPWSTR>(L"Phantom Dust pdnet: keeps this PC's player (scoreboards) when the game is reinstalled");
            c.CredentialBlobSize = static_cast<DWORD>(data.size());
            c.CredentialBlob = data.data();
            c.Persist = CRED_PERSIST_LOCAL_MACHINE;
            c.UserName = const_cast<LPWSTR>(L"pdnet");
            if (CredWriteW(&c, 0)) ++kept;
            else Log(L"identity: could not keep %s in Windows Credential Manager (error %lu)", file.c_str(), GetLastError());
        }
        if (restored || kept) Log(L"identity: %d file(s) put back, %d newly kept in Windows Credential Manager", restored, kept);
    }

    // Controllers the game can't see itself: it lists only Xbox / XInput pads (Windows.Gaming.Input.Gamepad), and its
    // sandbox can't open PlayStation, Nintendo or other HID controllers. While the game runs, SDL3 (SDL3.dll next to
    // pdlaunch.exe, loaded here so pdlaunch works without it) reads them, and every 4 ms their state goes into
    // LocalState\pdnet\pads.bin in Xbox layout (padshare.h) for pdnet's Gamepad class (gamepads.cpp). Xbox-type pads
    // are left out: the game reads those itself. Rumble the game asks for comes back through the same file.
    //   pdnet.ini: more_controllers=off turns it off.
    struct Sdl
    {
        HMODULE dll = nullptr;
#define PD_SDL(name) decltype(&::name) name = nullptr;
        PD_SDL(SDL_SetHint) PD_SDL(SDL_Init) PD_SDL(SDL_Quit) PD_SDL(SDL_PollEvent) PD_SDL(SDL_GetError) PD_SDL(SDL_OpenGamepad)
        PD_SDL(SDL_CloseGamepad) PD_SDL(SDL_GetRealGamepadType) PD_SDL(SDL_GetGamepadName) PD_SDL(SDL_GetGamepadButton)
        PD_SDL(SDL_GetGamepadAxis) PD_SDL(SDL_RumbleGamepad) PD_SDL(SDL_RumbleGamepadTriggers) PD_SDL(SDL_GetGamepads) PD_SDL(SDL_free)
#undef PD_SDL
        bool Load()
        {
            dll = LoadLibraryExW((g_dir + L"SDL3.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!dll) return false;
            bool ok = true;
#define PD_SDL(name) ok = ok && (name = reinterpret_cast<decltype(name)>(GetProcAddress(dll, #name))) != nullptr;
            PD_SDL(SDL_SetHint) PD_SDL(SDL_Init) PD_SDL(SDL_Quit) PD_SDL(SDL_PollEvent) PD_SDL(SDL_GetError) PD_SDL(SDL_OpenGamepad)
            PD_SDL(SDL_CloseGamepad) PD_SDL(SDL_GetRealGamepadType) PD_SDL(SDL_GetGamepadName) PD_SDL(SDL_GetGamepadButton)
            PD_SDL(SDL_GetGamepadAxis) PD_SDL(SDL_RumbleGamepad) PD_SDL(SDL_RumbleGamepadTriggers) PD_SDL(SDL_GetGamepads) PD_SDL(SDL_free)
#undef PD_SDL
            return ok;
        }
    };

    pdnet::pads::Shared* MapPads(const std::wstring& localState)
    {
        CreateDirectoryW((localState + L"\\pdnet").c_str(), nullptr);
        HANDLE file = CreateFileW((localState + pdnet::pads::kFile).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return nullptr;
        HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, sizeof(pdnet::pads::Shared), nullptr);
        CloseHandle(file);
        void* view = mapping ? MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(pdnet::pads::Shared)) : nullptr;
        if (mapping) CloseHandle(mapping);
        return static_cast<pdnet::pads::Shared*>(view);
    }

    // Has the game the keyboard focus? Its own window on top, or a child of the frame on top, or (seen on the developer's
    // PC, 124: a CoreWindow of no process could be found) Windows' app frame with no child of another process, and a title
    // that is empty or the game's. Another app's frame has its own CoreWindow and title.
    bool GameHasFocus(DWORD pid)
    {
        HWND top = GetForegroundWindow();
        DWORD owner = 0;
        if (!top) return false;
        GetWindowThreadProcessId(top, &owner);
        if (owner == pid) return true;
        struct Search { DWORD pid; bool found; } search{ pid, false };
        EnumChildWindows(top, [](HWND child, LPARAM param) -> BOOL
        {
            auto s = reinterpret_cast<Search*>(param);
            DWORD p = 0;
            GetWindowThreadProcessId(child, &p);
            if (p != s->pid) return TRUE;
            s->found = true;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&search));
        if (search.found) return true;
        wchar_t name[64] = {};
        if (!GetClassNameW(top, name, 64) || wcscmp(name, L"ApplicationFrameWindow") != 0) return false;
        wchar_t title[128] = {};
        GetWindowTextW(top, title, 128);
        return !title[0] || wcsstr(title, L"Phantom Dust");
    }

    // The keyboard for pdnet (padshare.h), every 8 ms until the game exits. Mouse buttons, the Windows keys and the
    // left / right copies of Shift, Ctrl and Alt are left out (the game's key events use the plain ones).
    void ReadKeyboard(DWORD pid, std::wstring localState)
    {
        pdnet::pads::Shared* shared = localState.empty() ? nullptr : MapPads(localState);
        HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!shared || !game)
        {
            Log(L"keyboard: not read for the game (error %lu)", GetLastError());
            if (game) CloseHandle(game);
            return;
        }
        bool told = false;
        while (WaitForSingleObject(game, 8) == WAIT_TIMEOUT)
        {
            bool focus = GameHasFocus(pid);
            for (int key = 0; key < 256; ++key)
            {
                bool wanted = key >= 8 && key < 0xFF && key != 0x5B && key != 0x5C && (key < 0xA0 || key > 0xA5);
                bool down = focus && wanted && (GetAsyncKeyState(key) & 0x8000) != 0;
                shared->keys[key] = down ? 1 : 0;
                if (down && !told)
                {
                    told = true;
                    Log(L"keyboard: first key seen (0x%02X)", key);
                }
            }
            shared->keyHeartbeat = GetTickCount();
        }
        shared->keyHeartbeat = 0;
        CloseHandle(game);
    }

    void ReadControllers(DWORD pid, std::wstring localState)
    {
        if (_wcsicmp(Setting(L"more_controllers", L"on").c_str(), L"off") == 0 || localState.empty()) return;
        Sdl sdl;
        if (!sdl.Load())
        {
            Log(L"controllers: no usable SDL3.dll next to pdlaunch.exe - only Xbox controllers");
            return;
        }
        pdnet::pads::Shared* shared = MapPads(localState);
        if (!shared)
        {
            Log(L"controllers: can't map %s%s (error %lu)", localState.c_str(), pdnet::pads::kFile, GetLastError());
            return;
        }
        shared->sequence = 0;
        memset(shared->pads, 0, sizeof(shared->pads));
        shared->magic = pdnet::pads::kMagic;
        shared->version = pdnet::pads::kVersion;

        sdl.SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"); // pdlaunch never has the focus
        // Xbox pads stay the game's: SDL doesn't take their HID connection, and doesn't read Windows.Gaming.Input (the
        // game's own API). XInput stays on so off-brand XInput pads show as Xbox type and are left out (no double input).
        sdl.SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0");
        sdl.SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
        if (!sdl.SDL_Init(SDL_INIT_GAMEPAD))
        {
            Log(L"controllers: SDL_Init failed (%S)", sdl.SDL_GetError());
            return;
        }
        Log(L"controllers: SDL3 ready (PlayStation, Nintendo and other controllers go to the game as Xbox-layout pads)");

        struct Slot { SDL_JoystickID sdlId = 0; SDL_Gamepad* pad = nullptr; uint32_t id = 0; SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN; char name[64] = {}; float rumble[4] = {}; DWORD rumbledAt = 0; };
        Slot slots[pdnet::pads::kMaxPads];
        uint32_t nextId = 1;
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);

        auto open = [&](SDL_JoystickID which)
        {
            for (auto& s : slots) if (s.pad && s.sdlId == which) return;
            SDL_Gamepad* pad = sdl.SDL_OpenGamepad(which);
            if (!pad) return;
            SDL_GamepadType type = sdl.SDL_GetRealGamepadType(pad);
            const char* name = sdl.SDL_GetGamepadName(pad);
            if (type == SDL_GAMEPAD_TYPE_XBOX360 || type == SDL_GAMEPAD_TYPE_XBOXONE)
            {
                Log(L"controllers: %S is an Xbox controller - the game reads it itself", name ? name : "?");
                sdl.SDL_CloseGamepad(pad);
                return;
            }
            for (auto& s : slots)
                if (!s.pad)
                {
                    s = Slot{};
                    s.sdlId = which;
                    s.pad = pad;
                    s.id = nextId++;
                    s.type = type;
                    strncpy_s(s.name, name ? name : "Controller", _TRUNCATE);
                    Log(L"controllers: %S connected (SDL type %d) - in the game as an Xbox-layout pad", s.name, static_cast<int>(type));
                    return;
                }
            sdl.SDL_CloseGamepad(pad); // all slots taken
        };

        while (!process || WaitForSingleObject(process, 4) == WAIT_TIMEOUT)
        {
            SDL_Event e;
            while (sdl.SDL_PollEvent(&e))
            {
                if (e.type == SDL_EVENT_GAMEPAD_ADDED) open(e.gdevice.which);
                else if (e.type == SDL_EVENT_GAMEPAD_REMOVED)
                    for (auto& s : slots)
                        if (s.pad && s.sdlId == e.gdevice.which)
                        {
                            Log(L"controllers: %S disconnected", s.name);
                            sdl.SDL_CloseGamepad(s.pad);
                            s = Slot{};
                        }
            }

            // The pads, in Xbox layout (SDL's south / east / west / north buttons are A / B / X / Y wherever they sit).
            shared->sequence = shared->sequence + 1;
            MemoryBarrier();
            for (int i = 0; i < pdnet::pads::kMaxPads; ++i)
            {
                Slot& s = slots[i];
                pdnet::pads::Pad& p = shared->pads[i];
                if (!s.pad) { p = {}; continue; }
                struct { SDL_GamepadButton sdl; uint32_t xbox; } const kButtons[] = {
                    { SDL_GAMEPAD_BUTTON_SOUTH, pdnet::pads::kA }, { SDL_GAMEPAD_BUTTON_EAST, pdnet::pads::kB },
                    { SDL_GAMEPAD_BUTTON_WEST, pdnet::pads::kX }, { SDL_GAMEPAD_BUTTON_NORTH, pdnet::pads::kY },
                    { SDL_GAMEPAD_BUTTON_START, pdnet::pads::kMenu }, { SDL_GAMEPAD_BUTTON_BACK, pdnet::pads::kView },
                    { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, pdnet::pads::kLeftShoulder }, { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, pdnet::pads::kRightShoulder },
                    { SDL_GAMEPAD_BUTTON_LEFT_STICK, pdnet::pads::kLeftThumbstick }, { SDL_GAMEPAD_BUTTON_RIGHT_STICK, pdnet::pads::kRightThumbstick },
                    { SDL_GAMEPAD_BUTTON_DPAD_UP, pdnet::pads::kDPadUp }, { SDL_GAMEPAD_BUTTON_DPAD_DOWN, pdnet::pads::kDPadDown },
                    { SDL_GAMEPAD_BUTTON_DPAD_LEFT, pdnet::pads::kDPadLeft }, { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, pdnet::pads::kDPadRight },
                    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, pdnet::pads::kPaddle1 }, { SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, pdnet::pads::kPaddle2 },
                    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, pdnet::pads::kPaddle3 }, { SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, pdnet::pads::kPaddle4 },
                };
                uint32_t buttons = 0;
                for (auto& b : kButtons) if (sdl.SDL_GetGamepadButton(s.pad, b.sdl)) buttons |= b.xbox;
                auto axis = [&](SDL_GamepadAxis a) { return std::clamp(sdl.SDL_GetGamepadAxis(s.pad, a) / 32767.0f, -1.0f, 1.0f); };
                p.id = s.id;
                p.type = static_cast<uint32_t>(s.type);
                p.buttons = buttons;
                p.leftX = axis(SDL_GAMEPAD_AXIS_LEFTX);
                p.leftY = -axis(SDL_GAMEPAD_AXIS_LEFTY); // SDL: down is positive; Xbox: up is
                p.rightX = axis(SDL_GAMEPAD_AXIS_RIGHTX);
                p.rightY = -axis(SDL_GAMEPAD_AXIS_RIGHTY);
                p.leftTrigger = (std::max)(0.0f, axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
                p.rightTrigger = (std::max)(0.0f, axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
                memcpy(p.name, s.name, sizeof(p.name));
            }
            shared->heartbeat = GetTickCount();
            MemoryBarrier();
            shared->sequence = shared->sequence + 1;

            // Rumble from the game (pdnet writes it with its own sequence number); renewed while it lasts.
            uint32_t before = shared->rumbleSequence;
            if (before & 1) continue;
            MemoryBarrier();
            pdnet::pads::Rumble rumble[pdnet::pads::kMaxPads];
            memcpy(rumble, const_cast<pdnet::pads::Rumble*>(shared->rumble), sizeof(rumble));
            MemoryBarrier();
            if (shared->rumbleSequence != before) continue;
            for (int i = 0; i < pdnet::pads::kMaxPads; ++i)
            {
                Slot& s = slots[i];
                if (!s.pad) continue;
                float want[4] = {};
                if (rumble[i].id == s.id) { want[0] = rumble[i].leftMotor; want[1] = rumble[i].rightMotor; want[2] = rumble[i].leftTrigger; want[3] = rumble[i].rightTrigger; }
                bool changed = memcmp(want, s.rumble, sizeof(want)) != 0;
                bool on = want[0] > 0 || want[1] > 0 || want[2] > 0 || want[3] > 0;
                if (!changed && !(on && GetTickCount() - s.rumbledAt > 500)) continue;
                auto level = [](float v) { return static_cast<Uint16>(std::clamp(v, 0.0f, 1.0f) * 65535.0f); };
                sdl.SDL_RumbleGamepad(s.pad, level(want[0]), level(want[1]), on ? 1000 : 0);
                sdl.SDL_RumbleGamepadTriggers(s.pad, level(want[2]), level(want[3]), on ? 1000 : 0);
                memcpy(s.rumble, want, sizeof(want));
                s.rumbledAt = GetTickCount();
            }
        }
        shared->sequence = shared->sequence + 1;
        memset(shared->pads, 0, sizeof(shared->pads));
        shared->sequence = shared->sequence + 1;
        for (auto& s : slots) if (s.pad) sdl.SDL_CloseGamepad(s.pad);
        sdl.SDL_Quit();
        if (process) CloseHandle(process);
    }

    // Discord: pdnet leaves LocalState\pdnet\discord-close.txt (message id, newline, "closed" text, UTF-8) while this
    // PC is alone in a lobby it announced (src\discord.cpp). If the game exits with it still there (closed from the
    // lobby, or a crash), the lobby ended with it: edit the message to "closed" with discord_webhook from pdnet.ini.
    void CloseDiscordMessage(const std::wstring& localState)
    {
        std::wstring path = localState + L"\\pdnet\\discord-close.txt";
        std::string note;
        if (FILE* f = nullptr; !_wfopen_s(&f, path.c_str(), L"rb") && f)
        {
            char buf[4096];
            for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;) note.append(buf, n);
            fclose(f);
        }
        else return;
        DeleteFileW(path.c_str());
        size_t newline = note.find('\n');
        std::wstring url = Setting(L"discord_webhook", L"");
        if (newline == std::string::npos || url.empty()) return;
        // The note exists only for a lobby whose host posted it (lobby posts on), so this edit follows that choice,
        // not this PC's own setting: this PC may have taken the lobby over from that host.
        std::string message = note.substr(0, newline), text = note.substr(newline + 1);

        std::string body = "{\"content\":\"";
        for (unsigned char c : text)
        {
            if (c == '"' || c == '\\') { body += '\\'; body += static_cast<char>(c); }
            else if (c < 0x20) { char esc[8]; sprintf_s(esc, "\\u%04x", c); body += esc; }
            else body += static_cast<char>(c);
        }
        body += "\",\"allowed_mentions\":{\"parse\":[]}}";

        URL_COMPONENTS parts{ sizeof(parts) };
        wchar_t host[256], urlPath[2048], extra[1024];
        parts.lpszHostName = host;
        parts.dwHostNameLength = _countof(host);
        parts.lpszUrlPath = urlPath;
        parts.dwUrlPathLength = _countof(urlPath);
        parts.lpszExtraInfo = extra;
        parts.dwExtraInfoLength = _countof(extra);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) return;
        std::wstring target = std::wstring(urlPath) + L"/messages/" + std::wstring(message.begin(), message.end()) + extra;
        DWORD status = 0, size = sizeof(status);
        HINTERNET session = WinHttpOpen(L"pdlaunch/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
        HINTERNET connect = session ? WinHttpConnect(session, host, parts.nPort, 0) : nullptr;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"PATCH", target.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
        if (request &&
            WinHttpSendRequest(request, L"Content-Type: application/json\r\n", static_cast<DWORD>(-1), body.data(), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
            WinHttpReceiveResponse(request, nullptr))
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
        Log(L"discord: the game exited in a lobby only this PC was in; marked its message closed (HTTP %lu)", status);
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
    }

    // Seconds this PC's clock is behind Epic's (the Date header of any answer from api.epicgames.dev; negative: ahead),
    // or false if Epic wasn't reached in a few seconds.
    bool ClockBehind(long long& seconds)
    {
        SYSTEMTIME server{};
        DWORD size = sizeof(server);
        bool ok = false;
        HINTERNET session = WinHttpOpen(L"pdlaunch/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
        if (session) WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);
        HINTERNET connect = session ? WinHttpConnect(session, L"api.epicgames.dev", INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"HEAD", L"/", nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
        if (request)
        {
            // A clock far enough off can make the server's certificate look expired or not yet valid; only the time is read.
            DWORD flags = SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
            ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr) &&
                 WinHttpQueryHeaders(request, WINHTTP_QUERY_DATE | WINHTTP_QUERY_FLAG_SYSTEMTIME, nullptr, &server, &size, nullptr);
        }
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        if (!ok) return false;
        SYSTEMTIME local;
        GetSystemTime(&local);
        FILETIME a, b;
        SystemTimeToFileTime(&server, &a);
        SystemTimeToFileTime(&local, &b);
        auto ticks = [](FILETIME t) { return static_cast<long long>((static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime); };
        seconds = (ticks(a) - ticks(b)) / 10000000;
        return true;
    }

    // Two PCs whose clocks are more than about a day apart can't connect: EOS makes the connection, then nothing
    // passes through it, even over Epic's relay (2026-10-29: a tester PC 31 h ahead never reached one player and
    // worked with others). Before pdnet (and EOS) start, this measures how far this PC's clock is from Epic's and
    // leaves the seconds to add in LocalState\pdnet\clock-offset.txt; pdnet makes the EOS SDK see the corrected time
    // (src\eosclock.h). The Windows clock is not changed: that needs administrator rights, and from here neither a
    // message box nor an admin prompt shows over the full-screen game (Experimental 59-63 tried). clock_check=off: no
    // offset, EOS uses the clock as it is.
    void CheckClock(const std::wstring& localState)
    {
        if (localState.empty()) return;
        std::wstring dir = localState + L"\\pdnet", file = dir + L"\\clock-offset.txt";
        DeleteFileW(file.c_str()); // from the last start
        if (_wcsicmp(Setting(L"clock_check", L"on").c_str(), L"off") == 0) return;
        long long behind = 0;
        if (!ClockBehind(behind))
        {
            Log(L"clock: Epic's time not reached - EOS uses this PC's clock as it is");
            return;
        }
        CreateDirectoryW(dir.c_str(), nullptr);
        if (FILE* f = nullptr; !_wfopen_s(&f, file.c_str(), L"wb") && f)
        {
            fprintf(f, "%lld\n", behind);
            fclose(f);
        }
        long long off = behind < 0 ? -behind : behind;
        Log(L"clock: %lld s %s Epic's%s", off, behind < 0 ? L"ahead of" : L"behind", off >= 60 ? L" - pdnet gives EOS the corrected time" : L"");
    }

    // ---- Updates (from GitHub Releases) -------------------------------------------------------------------------------
    // build.bat publishes every Experimental kit as a pre-release and every Stable as a release, each with a
    // secret-free "pdnet-update-<build>.zip" (no eos.ini, no Discord links; a Stable also carries the whole kit for first
    // installs) and the line
    // "sha256 pdnet-update-<build>.zip <hex>" in its notes. At game start, before pdnet is injected, CheckForUpdate
    // looks for a higher build on this kit's channel (update_channel=stable|experimental|off; by default the kit's own
    // kind, and off for development builds; an Experimental kit on the stable channel goes back to the newest Stable), downloads it into pdnet\update\ (inside the kit folder, so the files get
    // the folder's read access for the game's sandbox), checks the hash, moves what it replaces to
    // pdnet\update\backup\ (put back if the swap fails) and swaps the files in; after a good swap pdnet\update\ is deleted. eos.ini and pdnet.ini are never touched. A pdnet.dll that doesn't
    // match kit-version.txt (a test build installed by hand) is left alone too.

    // GET https://<host><path> into `body`; false if it failed or wasn't 200.
    bool HttpsGet(const wchar_t* host, const std::wstring& path, std::string& body, DWORD timeoutMs)
    {
        bool ok = false;
        HINTERNET session = WinHttpOpen(L"pdlaunch-updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
        if (session) WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
        HINTERNET connect = session ? WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
        if (request && WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", static_cast<DWORD>(-1), nullptr, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr))
        {
            DWORD status = 0, size = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
            char buf[65536];
            for (DWORD got = 0; WinHttpReadData(request, buf, sizeof(buf), &got) && got;) body.append(buf, got);
            ok = status == 200;
        }
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        return ok;
    }

    // The tag of a GitHub repo's newest release from the website: github.com/<repo>/releases/latest answers with a redirect
    // to .../releases/tag/<tag> (percent-encoded). Not the API, so it doesn't use the API's 60 lookups an hour (a "not
    // modified" API answer does, without a login: checked 2026-11-21). False when GitHub wasn't reached or the repo has no
    // release (the redirect has no /tag/).
    bool LatestReleaseTag(const std::wstring& repo, std::string& tag, unsigned long timeoutMs)
    {
        bool ok = false;
        HINTERNET session = WinHttpOpen(L"pdlaunch", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
        if (session) WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
        HINTERNET connect = session ? WinHttpConnect(session, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
        std::wstring path = L"/" + repo + L"/releases/latest";
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"HEAD", path.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
        DWORD noRedirects = WINHTTP_DISABLE_REDIRECTS;
        if (request) WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &noRedirects, sizeof(noRedirects));
        if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr))
        {
            DWORD status = 0, size = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
            wchar_t location[2048] = {};
            DWORD length = sizeof(location);
            if (status >= 300 && status < 400 &&
                WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location, &length, WINHTTP_NO_HEADER_INDEX))
            {
                std::wstring where = location;
                size_t at = where.find(L"/releases/tag/");
                if (at != std::wstring::npos)
                {
                    std::wstring t = where.substr(at + 14);
                    t = t.substr(0, t.find_first_of(L"?#"));
                    for (wchar_t c : t) tag += static_cast<char>(c < 0x80 ? c : '?'); // the URL's own (percent-encoded) form
                    ok = !tag.empty();
                }
            }
        }
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
        return ok;
    }

    // HttpsGet with a whole https:// URL (the mod catalog and downloads, modsync).
    bool UrlGet(const std::wstring& url, std::string& body, unsigned long timeoutMs)
    {
        URL_COMPONENTS parts{ sizeof(parts) };
        wchar_t host[256], path[2048], extra[1024];
        parts.lpszHostName = host;
        parts.dwHostNameLength = _countof(host);
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = _countof(path);
        parts.lpszExtraInfo = extra;
        parts.dwExtraInfoLength = _countof(extra);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) return false;
        return HttpsGet(host, std::wstring(path) + extra, body, timeoutMs);
    }

    // Lowercase hex digest (BCRYPT_MD5_ALGORITHM / BCRYPT_SHA256_ALGORITHM) of some bytes; empty on failure.
    std::string HashHex(LPCWSTR algorithm, const std::string& data)
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        std::string hex;
        if (BCryptOpenAlgorithmProvider(&alg, algorithm, nullptr, 0) != 0) return hex;
        DWORD length = 0, got = 0;
        BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&length), sizeof(length), &got, 0);
        std::vector<UCHAR> digest(length);
        if (BCryptHash(alg, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()), digest.data(), length) == 0)
            for (UCHAR b : digest) { char two[3]; sprintf_s(two, "%02x", b); hex += two; }
        BCryptCloseAlgorithmProvider(alg, 0);
        return hex;
    }

    std::string Sha256(const std::string& data) { return HashHex(BCRYPT_SHA256_ALGORITHM, data); }

    std::string ReadFileBytes(const std::wstring& path)
    {
        std::string data;
        if (FILE* f = nullptr; !_wfopen_s(&f, path.c_str(), L"rb") && f)
        {
            char buf[65536];
            for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;) data.append(buf, n);
            fclose(f);
        }
        return data;
    }

    // Runs a command line hidden and waits; true when it exits with 0.
    bool RunHidden(std::wstring command, DWORD timeoutMs)
    {
        STARTUPINFOW si{ sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
        DWORD exit = 1;
        if (WaitForSingleObject(pi.hProcess, timeoutMs) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &exit);
        else TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exit == 0;
    }

    void DeleteTree(const std::wstring& dir)
    {
        WIN32_FIND_DATAW fd;
        HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &fd);
        if (find != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::wstring name = fd.cFileName;
                if (name == L"." || name == L"..") continue;
                std::wstring path = dir + L"\\" + name;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) DeleteTree(path);
                else { SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileW(path.c_str()); }
            } while (FindNextFileW(find, &fd));
            FindClose(find);
        }
        RemoveDirectoryW(dir.c_str());
    }

    // What kit-version.txt says about this kit: its label, the pdnet.dll MD5 prefix it was built with, its build number.
    struct KitVersion { std::string label, md5; int build = 0; };
    KitVersion ReadKitVersion()
    {
        KitVersion v;
        std::string text = ReadFileBytes(g_dir + L"kit-version.txt"), line;
        if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
        size_t start = 0;
        for (int n = 0; start < text.size(); ++n)
        {
            size_t end = text.find('\n', start);
            line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (n == 0) v.label = line;
            else if (line.rfind("pdnet.dll MD5 ", 0) == 0) v.md5 = line.substr(14, 8);
            else if (line.rfind("Build: ", 0) == 0) v.build = atoi(line.c_str() + 7);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        for (auto& c : v.md5) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return v;
    }

    // Moves `from` over `to` (same volume: the file keeps the read access it got where it was made, inside the kit).
    bool Replace(const std::wstring& from, const std::wstring& to)
    {
        return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
    }

    void ClearOldUpdate();

    // An update just replaced pdlaunch.exe (CheckForUpdate): this start can hand over to the new one (wmain), so a newer
    // launcher's work (PDPL, for one) happens on the start that installed it, not only from the next.
    bool g_launcherReplaced = false;

    void CheckForUpdate()
    {
        ClearOldUpdate();
        KitVersion kit = ReadKitVersion();
        // Conceptual kits (build.bat conceptual, Conceptual branch only) have their own updates: pre-releases
        // "conceptual-<N>" with the asset pdnet-conceptual-<N>.zip. Other kits only take build-<N> / stable-<M> releases, and
        // a Conceptual kit only its own, so it can never become a master build. It ignores update_channel, which settings.ini
        // shares with the other kits (only conceptual_updates=off stops it).
        const bool conceptual = kit.label.find("Conceptual") != std::string::npos;
        if (conceptual && _wcsicmp(Setting(L"conceptual_updates", L"").c_str(), L"off") == 0) { Log(L"update: off (conceptual_updates=off)"); return; }
        // Stable is the public version since 2026-11-10 (the separate public channel and its Public kits are gone): a
        // "Public" kit, or update_channel=public left in a Public kit's pdnet.ini, follows Stable.
        std::wstring fallback = kit.label.find("Experimental") != std::string::npos ? L"experimental"
                              : kit.label.find("Stable") != std::string::npos || kit.label.find("Public") != std::string::npos ? L"stable" : L"off";
        std::wstring channel = conceptual ? L"conceptual" : Setting(L"update_channel", L"");
        for (auto& c : channel) c = static_cast<wchar_t>(towlower(c));
        if (channel == L"public") channel = L"stable";
        if (!conceptual && !channel.empty() && channel != L"stable" && channel != L"experimental" && channel != L"off")
        {
            // A typo in settings.ini must not count as stable, which would take an Experimental kit back to a Stable.
            Log(L"update: update_channel=%s isn't stable, experimental or off - using this kit's own (%s)", channel.c_str(), fallback.c_str());
            channel.clear();
        }
        if (channel.empty()) channel = fallback;
        if (channel == L"off") { Log(L"update: off (%s)", fallback == L"off" ? L"development build" : L"update_channel=off"); return; }
        bool experimental = channel == L"experimental" || conceptual; // Conceptual builds are pre-releases
        // Each channel takes only its own releases (tag build-<N> / stable-<M> / conceptual-<N>, which is in each asset's
        // download link): a kit switched to another channel gets that kind's label from its own release (a Stable's asset has
        // the same name as its Experimental's).
        std::string tagPrefix = conceptual ? "/download/conceptual-" : experimental ? "/download/build-" : "/download/stable-";
        const char* kindName = conceptual ? "Conceptual" : experimental ? "Experimental" : "Stable";
        std::string assetMarker = conceptual ? "/pdnet-conceptual-" : "/pdnet-update-";

        std::string md5 = HashHex(BCRYPT_MD5_ALGORITHM, ReadFileBytes(g_dir + L"pdnet.dll")).substr(0, 8);
        if (!kit.md5.empty() && md5 != kit.md5)
        {
            Log(L"update: skipped - pdnet.dll (%S) isn't this kit's (%S), a build installed by hand", md5.c_str(), kit.md5.c_str());
            return;
        }

        std::wstring repo = Setting(L"update_repo", L"SnowRyze/Phantom-Dust-Rebirth");
        // Newest first, 100 to a page. The experimental channel's newest is on the first page; a Stable can be far down
        // (Experimentals and Conceptual kits pile up on top of it: with 30 a page, stable-10 had dropped off the list and
        // Stable kits saw no Stable at all), so that channel reads on until it has seen one, 5 pages at most.
        std::string json;
        for (int page = 1; page <= 5; ++page)
        {
            std::string more;
            if (!HttpsGet(L"api.github.com", L"/repos/" + repo + L"/releases?per_page=100&page=" + std::to_wstring(page), more, 4000))
            {
                if (page > 1) break; // what was read so far still counts
                Log(L"update: GitHub not reached - not checked");
                return;
            }
            json += more;
            if (experimental || more.find(tagPrefix) != std::string::npos || more.find("\"tag_name\"") == std::string::npos) break;
        }
        // Releases come newest first; in each, "prerelease" comes before its assets. The highest build on the channel wins.
        int best = 0;
        std::string bestUrl;
        size_t bestAt = 0; // where its URL is: its release's notes (with the sha256 line) come after it
        bool pre = false;
        for (size_t at = 0;;)
        {
            size_t p = json.find("\"prerelease\":", at), a = json.find("\"browser_download_url\":", at);
            if (p == std::string::npos && a == std::string::npos) break;
            if (p != std::string::npos && (a == std::string::npos || p < a))
            {
                pre = json.compare(p + 13, 4, "true") == 0 || json.compare(p + 14, 4, "true") == 0;
                at = p + 13;
                continue;
            }
            size_t q1 = json.find('"', a + 23), q2 = q1 == std::string::npos ? q1 : json.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            std::string url = json.substr(q1 + 1, q2 - q1 - 1);
            at = q2;
            size_t name = url.rfind(assetMarker);
            if (name == std::string::npos || url.find(tagPrefix) == std::string::npos || (pre && !experimental)) continue;
            int build = atoi(url.c_str() + name + assetMarker.size());
            if (build > best) { best = build; bestUrl = url; bestAt = q2; }
        }
        // A kit of another kind than its channel (Options > Updates was switched) goes to that channel's newest build, even
        // when its build number is lower or the same: then it is that kind, with its label (the user, 2026-11-07, after
        // Public 1 switched to experimental stayed "Public 1": Experimental 133 is the same build).
        bool revert = kit.label.find(kindName) == std::string::npos;
        if (revert && best == 0)
        {
            Log(L"update: %s channel, but no %S build is published yet - keeping build %d", channel.c_str(), kindName, kit.build);
            return;
        }
        // (Going back installs even the same build number: that kit then is a Stable / an Experimental, with that kind's label.)
        if (!revert && best <= kit.build)
        {
            Log(L"update: up to date (build %d, %s channel)", kit.build, channel.c_str());
            return;
        }
        std::string asset = assetMarker.substr(1) + std::to_string(best) + ".zip";
        // From the same release: a Stable's asset has the same name as its Experimental's, with another kit-version.txt.
        size_t s = json.find("sha256 " + asset + " ", bestAt);
        std::string expected = s == std::string::npos ? std::string() : json.substr(s + 8 + asset.size(), 64);
        if (expected.size() != 64)
        {
            Log(L"update: build %d has no sha256 line for %S - not installed", best, asset.c_str());
            return;
        }

        if (revert) Log(L"update: %s channel - going to %S build %d (this kit is %S, build %d) - downloading", channel.c_str(), kindName, best, kit.label.c_str(), kit.build);
        else Log(L"update: build %d available (this kit is build %d) - downloading", best, kit.build);
        URL_COMPONENTS parts{ sizeof(parts) };
        wchar_t host[256], path[2048];
        parts.lpszHostName = host; parts.dwHostNameLength = _countof(host);
        parts.lpszUrlPath = path; parts.dwUrlPathLength = _countof(path);
        std::wstring wurl(bestUrl.begin(), bestUrl.end());
        std::string zip;
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts) || !HttpsGet(host, path, zip, 30000))
        {
            Log(L"update: download failed - not installed");
            return;
        }
        if (HashHex(BCRYPT_SHA256_ALGORITHM, zip) != expected)
        {
            Log(L"update: download doesn't match its sha256 - not installed");
            return;
        }

        std::wstring work = g_dir + L"update", fresh = work + L"\\new", backup = work + L"\\backup", file = work + L"\\download.zip";
        DeleteTree(fresh);
        CreateDirectoryW(work.c_str(), nullptr);
        CreateDirectoryW(fresh.c_str(), nullptr);
        if (FILE* f = nullptr; !_wfopen_s(&f, file.c_str(), L"wb") && f) { fwrite(zip.data(), 1, zip.size(), f); fclose(f); }
        wchar_t system[MAX_PATH];
        GetSystemDirectoryW(system, MAX_PATH);
        if (!RunHidden(L"\"" + std::wstring(system) + L"\\tar.exe\" -xf \"" + file + L"\" -C \"" + fresh + L"\"", 60000) ||
            GetFileAttributesW((fresh + L"\\pdnet\\pdnet.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            Log(L"update: couldn't unpack build %d - not installed", best);
            DeleteTree(fresh);
            DeleteFileW(file.c_str());
            return;
        }

        // Back up and replace every file the update has, except the player's own settings.
        DeleteTree(backup);
        CreateDirectoryW(backup.c_str(), nullptr);
        std::vector<std::wstring> done;
        bool failed = false;
        WIN32_FIND_DATAW fd;
        HANDLE find = FindFirstFileW((fresh + L"\\pdnet\\*").c_str(), &fd);
        if (find != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::wstring name = fd.cFileName;
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || _wcsicmp(name.c_str(), L"eos.ini") == 0 || _wcsicmp(name.c_str(), L"pdnet.ini") == 0) continue;
                std::wstring current = g_dir + name;
                bool exists = GetFileAttributesW(current.c_str()) != INVALID_FILE_ATTRIBUTES;
                // A running exe (this one) can be renamed though not overwritten: it moves to the backup like the rest.
                if (exists && !Replace(current, backup + L"\\" + name)) { failed = true; break; }
                if (!Replace(fresh + L"\\pdnet\\" + name, current)) { failed = true; if (exists) Replace(backup + L"\\" + name, current); break; }
                done.push_back(name);
            } while (FindNextFileW(find, &fd));
            FindClose(find);
        }
        if (failed)
        {
            for (auto& name : done) // put the old files back
                if (GetFileAttributesW((backup + L"\\" + name).c_str()) != INVALID_FILE_ATTRIBUTES) Replace(backup + L"\\" + name, g_dir + name);
            Log(L"update: replacing files failed (%lu) - the old version is back", GetLastError());
        }
        else
        {
            std::wstring setup = fresh + L"\\Setup.bat", parentSetup = g_dir + L"..\\Setup.bat";
            if (GetFileAttributesW(setup.c_str()) != INVALID_FILE_ATTRIBUTES && GetFileAttributesW(parentSetup.c_str()) != INVALID_FILE_ATTRIBUTES) Replace(setup, parentSetup);
            Log(L"update: installed build %d (%zu files; the old files are deleted)", best, done.size());
            for (auto& name : done)
                if (_wcsicmp(name.c_str(), L"pdlaunch.exe") == 0) g_launcherReplaced = true;
        }
        DeleteTree(fresh);
        DeleteFileW(file.c_str());
        if (!failed) DeleteTree(work); // the old pdlaunch.exe is still running: it goes at the next start (ClearOldUpdate)
    }

    // pdnet\update\ holds an update only while it is installed: anything left there (the previous pdlaunch.exe, which
    // can't be deleted while it runs, or files of an interrupted update) is deleted at the next start.
    void ClearOldUpdate()
    {
        std::wstring work = g_dir + L"update";
        if (GetFileAttributesW(work.c_str()) == INVALID_FILE_ATTRIBUTES) return;
        DeleteTree(work);
        Log(L"update: old update files %s", GetFileAttributesW(work.c_str()) == INVALID_FILE_ATTRIBUTES ? L"deleted" : L"partly deleted (some still in use)");
    }

    // The game's LocalState folder (pdnet builds the same path, src\log.cpp).
    std::wstring GameLocalState(HANDLE process)
    {
        wchar_t family[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
        UINT32 length = _countof(family);
        wchar_t profile[MAX_PATH];
        if (!GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH)) return {};
        // wine-uwp (Linux) keeps the package identity in its environment, which pdlaunch shares with the game (RunGame)
        if (GetPackageFamilyName(process, &length, family) != ERROR_SUCCESS &&
            !GetEnvironmentVariableW(L"WINE_UWP_PACKAGE_FAMILY_NAME", family, _countof(family))) return {};
        return std::wstring(profile) + L"\\AppData\\Local\\Packages\\" + family + L"\\LocalState";
    }
}

// Windows starts pdlaunch with the game because Setup attached it (IPackageDebugSettings::EnableDebugging), and that
// doesn't survive a restart: the game then starts without pdnet, until Setup is run again. So a per-user sign-in entry
// runs "pdlaunch.exe -attach", which sets it again (no administrator rights needed). KeepAttached adds the entry at each
// game start, so kits set up before it existed get it too; Remove.bat and Restore-XboxLive.bat delete it.
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunName[] = L"PhantomDust pdnet";

void KeepAttached()
{
    std::wstring command = L"\"" + g_dir + L"pdlaunch.exe\" -attach";
    wchar_t current[MAX_PATH * 2] = {};
    DWORD size = sizeof(current);
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunName, RRF_RT_REG_SZ, nullptr, current, &size) == ERROR_SUCCESS && command == current) return;
    LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunName, REG_SZ, command.c_str(), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    Log(L"attach: sign-in entry %s (%ld)", status == ERROR_SUCCESS ? L"set" : L"not set", status);
}

int Attach()
{
    constexpr wchar_t kFamily[] = L"Microsoft.MSEsper_8wekyb3d8bbwe";
    UINT32 count = 0, length = 0;
    LONG status = GetPackagesByPackageFamily(kFamily, &count, nullptr, &length, nullptr);
    if (status != ERROR_INSUFFICIENT_BUFFER || !count)
    {
        Log(L"attach: Phantom Dust isn't installed (%ld)", status);
        return 1;
    }
    std::vector<PWSTR> names(count);
    std::vector<wchar_t> buffer(length);
    if (GetPackagesByPackageFamily(kFamily, &count, names.data(), &length, buffer.data()) != ERROR_SUCCESS) return 1;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IPackageDebugSettings* settings = nullptr;
    HRESULT result = CoCreateInstance(CLSID_PackageDebugSettings, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&settings));
    std::wstring command = L"\"" + g_dir + L"pdlaunch.exe\"";
    if (settings)
    {
        for (UINT32 i = 0; i < count; ++i)
        {
            result = settings->EnableDebugging(names[i], command.c_str(), nullptr);
            Log(L"attach: %s -> 0x%08X", names[i], static_cast<unsigned>(result));
        }
        settings->Release();
    }
    else Log(L"attach: no package debug settings (0x%08X)", static_cast<unsigned>(result));
    CoUninitialize();
    return SUCCEEDED(result) ? 0 : 1;
}

// Linux (wine-uwp): Wine has no package debugger, so nothing starts pdlaunch with the game. wine-uwp starts pdlaunch in
// the game's place instead (setup-linux.sh writes UWP_EXE=pdlaunch.exe and PDLAUNCH_GAME=PDUWP.exe into local.env in
// the game's folder, where pdlaunch then is), and pdlaunch starts the game itself: suspended, with the same command
// line and environment (wine-uwp's package identity is in it), as Windows would have. The rest is as on Windows, except
// the kit's updates (setup-linux.sh does them before the game starts, then copies the kit into the game's folder) and
// SDL's controllers (Wine already gives the game every pad, so SDL would read them twice). Returns the game's process
// (its exit code is pdlaunch's, which wine-uwp passes on), or nullptr when PDLAUNCH_GAME isn't set or the game didn't start.
HANDLE RunGame(DWORD& pid, DWORD& tid)
{
    wchar_t game[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"PDLAUNCH_GAME", game, MAX_PATH) || !game[0]) return nullptr;
    std::wstring exe = game;
    if (exe.find(L':') == std::wstring::npos && exe[0] != L'\\') exe = g_dir + exe;
    // pdlaunch's own command line after its name (quoted or not) goes to the game
    const wchar_t* rest = GetCommandLineW();
    if (*rest == L'"') { rest = wcschr(rest + 1, L'"'); rest = rest ? rest + 1 : L""; }
    else while (*rest && *rest != L' ' && *rest != L'\t') ++rest;
    while (*rest == L' ' || *rest == L'\t') ++rest;
    std::wstring command = L"\"" + exe + L"\"" + (*rest ? L" " + std::wstring(rest) : L"");
    SetEnvironmentVariableW(L"PDLAUNCH_GAME", nullptr);
    SetEnvironmentVariableW(L"UWP_EXE", game);
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi))
    {
        Log(L"linux: %s couldn't be started (%lu)", exe.c_str(), GetLastError());
        return nullptr;
    }
    CloseHandle(pi.hThread);
    pid = pi.dwProcessId;
    tid = pi.dwThreadId;
    Log(L"linux: started %s suspended (wine-uwp has no package debugger to start pdlaunch with it)", game);
    return pi.hProcess;
}

int wmain(int argc, wchar_t** argv)
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g_dir = exe;
    g_dir = g_dir.substr(0, g_dir.find_last_of(L'\\') + 1);

    // pdlaunch.exe -update: only the update check (to update without starting the game, and to test the updater).
    if (argc == 2 && _wcsicmp(argv[1], L"-update") == 0)
    {
        CheckForUpdate();
        return 0;
    }

    if (argc == 2 && _wcsicmp(argv[1], L"-attach") == 0) return Attach();

    // pdlaunch.exe -syncmods <LocalState folder> [catalog URL or file] [game folder]: only the mod sync with its catalog
    // downloads and map slots (testing; modsync.h), logged to pdlaunch.log.
    if (argc >= 3 && argc <= 5 && _wcsicmp(argv[1], L"-syncmods") == 0)
    {
        modsync::Sync(argv[2], Log, &kModNet, argc >= 4 && argv[3][0] ? std::wstring(argv[3]) : Setting(L"mod_list_url", kModListUrl), argc == 5 ? std::wstring(argv[4]) : std::wstring());
        return 0;
    }

    DWORD pid = 0, tid = 0;
    bool handedOver = false; // started by the previous pdlaunch.exe right after it installed this one (below)
    HANDLE linuxGame = RunGame(pid, tid); // Linux: pdlaunch starts the game itself (RunGame)
    for (int i = 1; i < argc && !linuxGame; ++i)
    {
        if (i + 1 < argc && _wcsicmp(argv[i], L"-p") == 0) pid = wcstoul(argv[i + 1], nullptr, 10);
        if (i + 1 < argc && _wcsicmp(argv[i], L"-tid") == 0) tid = wcstoul(argv[i + 1], nullptr, 10);
        if (_wcsicmp(argv[i], L"-handover") == 0) handedOver = true;
    }
    if (!pid || !tid)
    {
        // pdlaunch has no console window (it runs in the background while the game is open), so this goes in a box.
        MessageBoxW(nullptr, L"pdlaunch is started by Windows when Phantom Dust starts (after Setup.bat). Start the game the normal way.", L"pdlaunch", MB_OK | MB_ICONINFORMATION);
        return 2;
    }

    // Windows starts pdlaunch for every process of the game's package, including its RuntimeBroker.exe helper, which
    // outlives the game: only PDUWP.exe gets pdnet; anything else just continues, and pdlaunch exits at once.
    {
        wchar_t image[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        HANDLE target = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        bool queried = target && QueryFullProcessImageNameW(target, 0, image, &size);
        if (target) CloseHandle(target);
        const wchar_t* file = wcsrchr(image, L'\\');
        file = file ? file + 1 : image;
        if (queried && _wcsicmp(file, L"PDUWP.exe") != 0)
        {
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
            if (thread)
            {
                ResumeThread(thread);
                CloseHandle(thread);
            }
            Log(L"%s (pid %lu) is not the game - left alone", file, pid);
            return 0;
        }
    }

    Log(handedOver ? L"game started (pid %lu, main thread %lu) - handed over by the previous pdlaunch after its update" : L"game started (pid %lu, main thread %lu)", pid, tid);
    if (!linuxGame) KeepAttached(); // (Windows' sign-in entry; wine-uwp starts pdlaunch every time)
    HANDLE game = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    std::wstring localState = game ? GameLocalState(game) : std::wstring();
    if (!localState.empty()) DeleteFileW(PastTitleFile(localState).c_str()); // from the last start: pdnet writes it again
    if (!handedOver && !linuxGame) CheckForUpdate(); // (on Linux setup-linux.sh has done it) before anything loads pdnet.dll, SDL3.dll or the EOS SDK, which it may replace
    // The update replaced pdlaunch.exe: the new one does the rest of this start (the game is still suspended; nothing is
    // loaded into it yet). If it can't be started, this one carries on as before.
    if (g_launcherReplaced && !handedOver)
    {
        std::wstring command = L"\"" + g_dir + L"pdlaunch.exe\" -p " + std::to_wstring(pid) + L" -tid " + std::to_wstring(tid) + L" -handover";
        STARTUPINFOW si{ sizeof(si) };
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, g_dir.c_str(), &si, &pi))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (game) CloseHandle(game);
            Log(L"update: handed this start over to the new pdlaunch.exe (pid %lu)", pi.dwProcessId);
            return 0;
        }
        Log(L"update: the new pdlaunch.exe couldn't be started (%lu) - this one carries on", GetLastError());
    }
    CheckClock(localState); // before pdnet starts EOS, which reads the offset
    KeepIdentity(localState, true); // before the game runs: the same player as always
    std::thread controllers; // until the game exits
    if (!linuxGame) controllers = std::thread(ReadControllers, pid, localState);
    std::thread keyboard(ReadKeyboard, pid, localState); // until the game exits
    bool injected = Inject(pid);
    if (injected) InjectPdpl(pid, localState);
    std::thread liveMods; // until the game exits
    if (injected && !localState.empty()) liveMods = std::thread(WatchLiveMods, pid, localState);

    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
    DWORD previous = thread ? ResumeThread(thread) : static_cast<DWORD>(-1);
    if (thread) CloseHandle(thread);
    Log(L"game resumed (suspend count was %ld)%s", static_cast<long>(previous), injected ? L"" : L" WITHOUT pdnet");
    WatchVolume(pid, localState, injected); // until the game exits (returns at once when there is nothing to do for the volume)
    if (game)
    {
        WaitForSingleObject(game, INFINITE);
        CloseHandle(game);
        if (injected && !localState.empty()) CloseDiscordMessage(localState);
    }
    KeepIdentity(localState, false); // a first sign-in made its credential while the game ran
    if (controllers.joinable()) controllers.join();
    keyboard.join();
    if (liveMods.joinable()) liveMods.join();
    if (linuxGame) // wine-uwp passes the game's exit code on
    {
        WaitForSingleObject(linuxGame, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(linuxGame, &code);
        CloseHandle(linuxGame);
        return static_cast<int>(code);
    }
    return injected ? 0 : 1;
}
