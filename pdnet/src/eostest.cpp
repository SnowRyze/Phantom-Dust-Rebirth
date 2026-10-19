#include "pch.h"
#include "eostest.h"
#include "credstore.h"
#include "log.h"

#ifdef PDNET_NO_EOS

namespace pdnet
{
    void StartEosTest()
    {
        Log(L"eos-test: this pdnet build was compiled without the EOS SDK");
    }
}

#else

#include <eos_sdk.h>
#include <eos_logging.h>
#include <eos_connect.h>
#include <eos_p2p.h>
#include <eos_lobby.h>
#include <fstream>
#include <thread>

namespace pdnet
{
    namespace
    {
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

        // eos.ini next to the DLL: ProductId=, SandboxId=, DeploymentId=, ClientId=, ClientSecret=
        std::map<std::string, std::string> ReadCredentials(const std::wstring& path)
        {
            std::map<std::string, std::string> values;
            std::ifstream in(path);
            for (std::string line; std::getline(in, line);)
            {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                auto eq = line.find('=');
                if (eq != std::string::npos) values[line.substr(0, eq)] = line.substr(eq + 1);
            }
            return values;
        }

        // The SDK is loaded at run time so the game still starts when the DLL isn't there.
        struct Sdk
        {
            HMODULE module = nullptr;
#define EOS_FN(name) decltype(&::name) name = nullptr
            EOS_FN(EOS_Initialize);
            EOS_FN(EOS_Shutdown);
            EOS_FN(EOS_Logging_SetCallback);
            EOS_FN(EOS_Logging_SetLogLevel);
            EOS_FN(EOS_Platform_Create);
            EOS_FN(EOS_Platform_Tick);
            EOS_FN(EOS_Platform_Release);
            EOS_FN(EOS_Platform_GetConnectInterface);
            EOS_FN(EOS_Platform_GetP2PInterface);
            EOS_FN(EOS_Platform_GetLobbyInterface);
            EOS_FN(EOS_Connect_CreateDeviceId);
            EOS_FN(EOS_Connect_Login);
            EOS_FN(EOS_Connect_CreateUser);
            EOS_FN(EOS_ProductUserId_ToString);
            EOS_FN(EOS_P2P_QueryNATType);
            EOS_FN(EOS_Lobby_CreateLobby);
            EOS_FN(EOS_Lobby_DestroyLobby);
            EOS_FN(EOS_EResult_ToString);
#undef EOS_FN

            bool Load(const std::wstring& path)
            {
                module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (!module) return false;
                bool ok = true;
#define EOS_LOAD(name) ok &= (name = reinterpret_cast<decltype(name)>(GetProcAddress(module, #name))) != nullptr
                EOS_LOAD(EOS_Initialize);
                EOS_LOAD(EOS_Shutdown);
                EOS_LOAD(EOS_Logging_SetCallback);
                EOS_LOAD(EOS_Logging_SetLogLevel);
                EOS_LOAD(EOS_Platform_Create);
                EOS_LOAD(EOS_Platform_Tick);
                EOS_LOAD(EOS_Platform_Release);
                EOS_LOAD(EOS_Platform_GetConnectInterface);
                EOS_LOAD(EOS_Platform_GetP2PInterface);
                EOS_LOAD(EOS_Platform_GetLobbyInterface);
                EOS_LOAD(EOS_Connect_CreateDeviceId);
                EOS_LOAD(EOS_Connect_Login);
                EOS_LOAD(EOS_Connect_CreateUser);
                EOS_LOAD(EOS_ProductUserId_ToString);
                EOS_LOAD(EOS_P2P_QueryNATType);
                EOS_LOAD(EOS_Lobby_CreateLobby);
                EOS_LOAD(EOS_Lobby_DestroyLobby);
                EOS_LOAD(EOS_EResult_ToString);
#undef EOS_LOAD
                return ok;
            }

            std::wstring Result(EOS_EResult r) const { return Wide(EOS_EResult_ToString(r)); }
        };

        Sdk g_sdk;

        void EOS_CALL OnSdkLog(const EOS_LogMessage* m)
        {
            Log(L"eos-sdk [%s] %s", Wide(m->Category).c_str(), Wide(m->Message).c_str());
        }

        // Every EOS call completes through a callback during EOS_Platform_Tick, all on this one thread.
        template <typename Info>
        struct Completion
        {
            bool done = false;
            Info info{};
            std::string lobbyId; // copied out; EOS strings are only valid inside the callback

            static void EOS_CALL Callback(const Info* data)
            {
                auto self = static_cast<Completion*>(data->ClientData);
                self->info = *data;
                if constexpr (requires { data->LobbyId; })
                    if (data->LobbyId) self->lobbyId = data->LobbyId;
                self->done = true;
            }
        };

        template <typename Info>
        bool TickUntil(EOS_HPlatform platform, const Completion<Info>& c, uint32_t timeoutMs)
        {
            auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (!c.done && std::chrono::steady_clock::now() < end)
            {
                g_sdk.EOS_Platform_Tick(platform);
                Sleep(20);
            }
            return c.done;
        }

        std::wstring UserIdString(EOS_ProductUserId id)
        {
            char buf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
            int32_t len = sizeof(buf);
            return g_sdk.EOS_ProductUserId_ToString(id, buf, &len) == EOS_EResult::EOS_Success ? Wide(buf) : L"?";
        }

        const wchar_t* NatName(EOS_ENATType t)
        {
            switch (t)
            {
            case EOS_ENATType::EOS_NAT_Open: return L"Open";
            case EOS_ENATType::EOS_NAT_Moderate: return L"Moderate";
            case EOS_ENATType::EOS_NAT_Strict: return L"Strict";
            default: return L"Unknown";
            }
        }

        void Run()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            std::wstring dir = ModuleDir();
            std::map<std::wstring, std::wstring> summary;
            auto step = [&](const wchar_t* name, const std::wstring& result) { summary[name] = result; Log(L"eos-test: %s: %s", name, result.c_str()); };
            auto finish = [&]
            {
                std::wstring line;
                for (const wchar_t* k : { L"load", L"init", L"platform", L"deviceid", L"login", L"nat", L"lobby" })
                    line += std::wstring(k) + L"=" + (summary.count(k) ? summary[k] : L"skipped") + L"  ";
                Log(L"eos-test: SUMMARY %s", line.c_str());
            };

            // 1. SDK
            if (!g_sdk.Load(dir + L"EOSSDK-Win64-Shipping.dll"))
            {
                step(L"load", L"FAIL - EOSSDK-Win64-Shipping.dll missing or incomplete in " + dir + L" (error " + std::to_wstring(GetLastError()) + L")");
                return finish();
            }
            step(L"load", L"ok");

            // The sandbox can't use Windows Credential Manager, where EOS keeps the device ID; give EOS a private store.
            std::wstring eosDir = LocalStatePath() + L"\\eos";
            CreateDirectoryW(eosDir.c_str(), nullptr);
            RedirectCredentialStore(g_sdk.module, eosDir + L"\\credentials");

            EOS_InitializeOptions init{};
            init.ApiVersion = EOS_INITIALIZE_API_LATEST;
            init.ProductName = "PhantomDust-pdnet";
            init.ProductVersion = "eos-test-1";
            EOS_EResult r = g_sdk.EOS_Initialize(&init);
            if (r != EOS_EResult::EOS_Success && r != EOS_EResult::EOS_AlreadyConfigured)
            {
                step(L"init", L"FAIL - " + g_sdk.Result(r));
                return finish();
            }
            step(L"init", L"ok");
            g_sdk.EOS_Logging_SetCallback(OnSdkLog);
            g_sdk.EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Warning);

            // 2. Platform
            auto creds = ReadCredentials(dir + L"eos.ini");
            for (const char* key : { "ProductId", "SandboxId", "DeploymentId", "ClientId", "ClientSecret" })
            {
                if (creds[key].empty())
                {
                    step(L"platform", L"FAIL - eos.ini next to the DLL is missing or has no " + Wide(key));
                    g_sdk.EOS_Shutdown();
                    return finish();
                }
            }
            std::wstring cacheDir = eosDir + L"\\cache";
            CreateDirectoryW(cacheDir.c_str(), nullptr);
            std::string cache = winrt::to_string(cacheDir);

            EOS_Platform_Options options{};
            options.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
            options.ProductId = creds["ProductId"].c_str();
            options.SandboxId = creds["SandboxId"].c_str();
            options.DeploymentId = creds["DeploymentId"].c_str();
            options.ClientCredentials.ClientId = creds["ClientId"].c_str();
            options.ClientCredentials.ClientSecret = creds["ClientSecret"].c_str();
            options.bIsServer = EOS_FALSE;
            options.Flags = EOS_PF_DISABLE_OVERLAY;
            options.CacheDirectory = cache.c_str();
            EOS_HPlatform platform = g_sdk.EOS_Platform_Create(&options);
            if (!platform)
            {
                step(L"platform", L"FAIL - EOS_Platform_Create returned null (see eos-sdk lines above)");
                g_sdk.EOS_Shutdown();
                return finish();
            }
            step(L"platform", L"ok");

            auto shutdown = [&]
            {
                g_sdk.EOS_Platform_Release(platform);
                g_sdk.EOS_Shutdown();
                finish();
            };
            EOS_HConnect connect = g_sdk.EOS_Platform_GetConnectInterface(platform);

            // 3. Device ID (an anonymous per-PC identity; "already exists" on later runs is fine)
            EOS_Connect_CreateDeviceIdOptions deviceOpts{};
            deviceOpts.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
            deviceOpts.DeviceModel = "PC";
            Completion<EOS_Connect_CreateDeviceIdCallbackInfo> device;
            g_sdk.EOS_Connect_CreateDeviceId(connect, &deviceOpts, &device, decltype(device)::Callback);
            if (!TickUntil(platform, device, 15000))
            {
                step(L"deviceid", L"FAIL - no response in 15 s (blocked network?)");
                return shutdown();
            }
            if (device.info.ResultCode != EOS_EResult::EOS_Success && device.info.ResultCode != EOS_EResult::EOS_DuplicateNotAllowed)
            {
                step(L"deviceid", L"FAIL - " + g_sdk.Result(device.info.ResultCode));
                return shutdown();
            }
            step(L"deviceid", device.info.ResultCode == EOS_EResult::EOS_Success ? L"ok (created)" : L"ok (already existed)");

            // 4. Log in with it; the first time, EOS asks us to create the product user.
            EOS_Connect_Credentials credentials{};
            credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
            credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
            EOS_Connect_UserLoginInfo loginInfo{};
            loginInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
            loginInfo.DisplayName = "pdnet-test";
            EOS_Connect_LoginOptions loginOpts{};
            loginOpts.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
            loginOpts.Credentials = &credentials;
            loginOpts.UserLoginInfo = &loginInfo;
            Completion<EOS_Connect_LoginCallbackInfo> login;
            g_sdk.EOS_Connect_Login(connect, &loginOpts, &login, decltype(login)::Callback);
            if (!TickUntil(platform, login, 15000))
            {
                step(L"login", L"FAIL - no response in 15 s");
                return shutdown();
            }
            EOS_ProductUserId user = login.info.LocalUserId;
            if (login.info.ResultCode == EOS_EResult::EOS_InvalidUser && login.info.ContinuanceToken)
            {
                EOS_Connect_CreateUserOptions createOpts{};
                createOpts.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
                createOpts.ContinuanceToken = login.info.ContinuanceToken;
                Completion<EOS_Connect_CreateUserCallbackInfo> created;
                g_sdk.EOS_Connect_CreateUser(connect, &createOpts, &created, decltype(created)::Callback);
                if (!TickUntil(platform, created, 15000) || created.info.ResultCode != EOS_EResult::EOS_Success)
                {
                    step(L"login", L"FAIL - CreateUser: " + (created.done ? g_sdk.Result(created.info.ResultCode) : std::wstring(L"no response")));
                    return shutdown();
                }
                user = created.info.LocalUserId;
                step(L"login", L"ok (new product user " + UserIdString(user) + L")");
            }
            else if (login.info.ResultCode != EOS_EResult::EOS_Success)
            {
                step(L"login", L"FAIL - " + g_sdk.Result(login.info.ResultCode) + L" (check the client policy allows Connect logins)");
                return shutdown();
            }
            else
            {
                step(L"login", L"ok (product user " + UserIdString(user) + L")");
            }

            // 5. NAT type as EOS sees it (a query only)
            EOS_P2P_QueryNATTypeOptions natOpts{};
            natOpts.ApiVersion = EOS_P2P_QUERYNATTYPE_API_LATEST;
            Completion<EOS_P2P_OnQueryNATTypeCompleteInfo> nat;
            g_sdk.EOS_P2P_QueryNATType(g_sdk.EOS_Platform_GetP2PInterface(platform), &natOpts, &nat, decltype(nat)::Callback);
            if (!TickUntil(platform, nat, 15000))
                step(L"nat", L"FAIL - no response in 15 s");
            else if (nat.info.ResultCode != EOS_EResult::EOS_Success)
                step(L"nat", L"FAIL - " + g_sdk.Result(nat.info.ResultCode) + L" (check the client policy allows P2P)");
            else
                step(L"nat", std::wstring(L"ok (") + NatName(nat.info.NATType) + L")");

            // 6. Create a lobby with presence off (no Epic account features), then destroy it.
            EOS_HLobby lobbies = g_sdk.EOS_Platform_GetLobbyInterface(platform);
            EOS_Lobby_CreateLobbyOptions lobbyOpts{};
            lobbyOpts.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
            lobbyOpts.LocalUserId = user;
            lobbyOpts.MaxLobbyMembers = 4;
            lobbyOpts.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
            lobbyOpts.bPresenceEnabled = EOS_FALSE;
            lobbyOpts.bAllowInvites = EOS_FALSE;
            lobbyOpts.BucketId = "pdnet-eos-test";
            Completion<EOS_Lobby_CreateLobbyCallbackInfo> lobby;
            g_sdk.EOS_Lobby_CreateLobby(lobbies, &lobbyOpts, &lobby, decltype(lobby)::Callback);
            if (!TickUntil(platform, lobby, 15000))
            {
                step(L"lobby", L"FAIL - no response in 15 s");
            }
            else if (lobby.info.ResultCode != EOS_EResult::EOS_Success)
            {
                step(L"lobby", L"FAIL - " + g_sdk.Result(lobby.info.ResultCode) + L" (check the client policy allows Lobbies)");
            }
            else
            {
                EOS_Lobby_DestroyLobbyOptions destroyOpts{};
                destroyOpts.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
                destroyOpts.LocalUserId = user;
                destroyOpts.LobbyId = lobby.lobbyId.c_str();
                Completion<EOS_Lobby_DestroyLobbyCallbackInfo> destroyed;
                g_sdk.EOS_Lobby_DestroyLobby(lobbies, &destroyOpts, &destroyed, decltype(destroyed)::Callback);
                TickUntil(platform, destroyed, 10000);
                step(L"lobby", L"ok (created " + Wide(lobby.lobbyId.c_str()) + L", destroy: " +
                    (destroyed.done ? g_sdk.Result(destroyed.info.ResultCode) : std::wstring(L"no response")) + L")");
            }
            shutdown();
        }
    }

    void StartEosTest()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            Log(L"eos-test: starting");
            std::thread([]
            {
                try { Run(); }
                catch (...) { Log(L"eos-test: crashed with 0x%08X", winrt::to_hresult()); }
            }).detach();
        });
    }
}

#endif
