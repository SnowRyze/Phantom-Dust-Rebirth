#include "pch.h"
#ifndef PDNET_NO_EOS
#include "eossdk.h"
#include "credstore.h"
#include "eosclock.h"
#include "config.h"
#include "log.h"
#include <fstream>

namespace pdnet::eossdk
{
    namespace
    {
        void EOS_CALL OnSdkLog(const EOS_LogMessage* m)
        {
            Log(L"eos-sdk [%s] %s", Wide(m->Category).c_str(), Wide(m->Message).c_str());
        }
    }

    std::wstring Wide(const char* s)
    {
        return s ? std::wstring{ winrt::to_hstring(s) } : std::wstring{};
    }

    std::wstring ModuleDir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&ModuleDir), &self);
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(self, path, MAX_PATH);
        std::wstring dir = path;
        return dir.substr(0, dir.find_last_of(L'\\') + 1);
    }

    const Api* Load(const std::wstring& dir)
    {
        static std::mutex lock;
        static Api api;
        static bool tried = false, ok = false;
        std::lock_guard guard(lock);
        if (tried) return ok ? &api : nullptr;
        tried = true;

        api.module = LoadLibraryExW((dir + L"EOSSDK-Win64-Shipping.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!api.module)
        {
            Log(L"eos: EOSSDK-Win64-Shipping.dll not found in %s (error %lu)", dir.c_str(), GetLastError());
            return nullptr;
        }
        ok = true;
#define PDNET_EOS_RESOLVE(name)                                                                     \
        if (!(api.name = reinterpret_cast<decltype(api.name)>(GetProcAddress(api.module, #name)))) \
        {                                                                                           \
            Log(L"eos: SDK is missing %S (wrong SDK version?)", #name);                             \
            ok = false;                                                                             \
        }
        PDNET_EOS_FUNCTIONS(PDNET_EOS_RESOLVE)
#undef PDNET_EOS_RESOLVE
        if (!ok) return nullptr;
        int lobbyFunctions = 0;
#define PDNET_EOS_COUNT(name) ++lobbyFunctions;
        PDNET_EOS_LOBBY_FUNCTIONS(PDNET_EOS_COUNT)
#undef PDNET_EOS_COUNT
        Log(L"eos: SDK loaded, all functions resolved (%d lobby functions)", lobbyFunctions);

        std::wstring eosDir = LocalStatePath() + L"\\eos";
        CreateDirectoryW(eosDir.c_str(), nullptr);
        RedirectCredentialStore(api.module, eosDir + L"\\credentials");
        CorrectEosClock(api.module); // before EOS_Initialize: a PC clock far off breaks P2P links (eosclock.h)
        return &api;
    }

    bool Initialize(const Api& api, const char* productVersion)
    {
        EOS_InitializeOptions init{};
        init.ApiVersion = EOS_INITIALIZE_API_LATEST;
        init.ProductName = "PhantomDust-pdnet";
        init.ProductVersion = productVersion;
        EOS_EResult r = api.EOS_Initialize(&init);
        if (r != EOS_EResult::EOS_Success && r != EOS_EResult::EOS_AlreadyConfigured)
        {
            Log(L"eos: EOS_Initialize failed: %s", Result(api, r).c_str());
            return false;
        }
        api.EOS_Logging_SetCallback(OnSdkLog);
        api.EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Warning);
        if (GetConfig().voiceDebug) api.EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_RTC, EOS_ELogLevel::EOS_LOG_Info);
        return true;
    }

    std::string ReadCredentials(const std::wstring& dir, std::map<std::string, std::string>& out)
    {
        std::ifstream in(dir + L"eos.ini");
        for (std::string line; std::getline(in, line);)
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq != std::string::npos) out[line.substr(0, eq)] = line.substr(eq + 1);
        }
        for (const char* key : { "ProductId", "SandboxId", "DeploymentId", "ClientId", "ClientSecret" })
            if (out[key].empty()) return key;
        return {};
    }

    EOS_HPlatform CreatePlatform(const Api& api, const std::map<std::string, std::string>& creds)
    {
        std::wstring cacheDir = LocalStatePath() + L"\\eos\\cache";
        CreateDirectoryW(cacheDir.c_str(), nullptr);
        std::string cache = winrt::to_string(cacheDir);

        EOS_Platform_Options options{};
        options.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
        options.ProductId = creds.at("ProductId").c_str();
        options.SandboxId = creds.at("SandboxId").c_str();
        options.DeploymentId = creds.at("DeploymentId").c_str();
        options.ClientCredentials.ClientId = creds.at("ClientId").c_str();
        options.ClientCredentials.ClientSecret = creds.at("ClientSecret").c_str();
        options.bIsServer = EOS_FALSE;
        options.Flags = EOS_PF_DISABLE_OVERLAY;
        options.CacheDirectory = cache.c_str();
        // Voice (voice_chat=on): without RTCOptions EOS has no voice at all. Rooms stay up while the game window
        // isn't focused. EOS plays and records through xaudio2_9redist.dll from the kit, next to pdnet.
        std::string xaudio = winrt::to_string(ModuleDir() + L"xaudio2_9redist.dll");
        EOS_Windows_RTCOptions windowsRtc{ EOS_WINDOWS_RTCOPTIONS_API_LATEST, xaudio.c_str() };
        EOS_Platform_RTCOptions rtc{};
        rtc.ApiVersion = EOS_PLATFORM_RTCOPTIONS_API_LATEST;
        rtc.PlatformSpecificOptions = GetFileAttributesW((ModuleDir() + L"xaudio2_9redist.dll").c_str()) != INVALID_FILE_ATTRIBUTES ? &windowsRtc : nullptr;
        rtc.BackgroundMode = EOS_ERTCBackgroundMode::EOS_RTCBM_KeepRoomsAlive;
        if (GetConfig().voiceChat)
        {
            options.RTCOptions = &rtc;
            Log(L"eos: voice enabled (%s)", rtc.PlatformSpecificOptions ? L"xaudio2_9redist.dll from the kit" : L"no xaudio2_9redist.dll next to pdnet - EOS's default");
        }
        return api.EOS_Platform_Create(&options);
    }

    std::wstring Result(const Api& api, EOS_EResult r)
    {
        return Wide(api.EOS_EResult_ToString(r));
    }

    std::wstring UserId(const Api& api, EOS_ProductUserId id)
    {
        char buf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
        int32_t len = sizeof(buf);
        return id && api.EOS_ProductUserId_ToString(id, buf, &len) == EOS_EResult::EOS_Success ? Wide(buf) : L"?";
    }
}
#endif
