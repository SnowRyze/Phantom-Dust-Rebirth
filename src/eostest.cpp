#include "pch.h"
#include "eostest.h"
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

#include "eossdk.h"
#include <thread>

namespace pdnet
{
    namespace
    {
        using namespace eossdk;

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
            const Api* sdk = Load(dir);
            if (!sdk)
            {
                step(L"load", L"FAIL - see eos: lines above");
                return finish();
            }
            const Api& api = *sdk;
            step(L"load", L"ok");
            if (!Initialize(api, "eos-test-1"))
            {
                step(L"init", L"FAIL - see eos: lines above");
                return finish();
            }
            step(L"init", L"ok");

            // 2. Platform
            std::map<std::string, std::string> creds;
            if (auto missing = ReadCredentials(dir, creds); !missing.empty())
            {
                step(L"platform", L"FAIL - eos.ini next to the DLL is missing or has no " + Wide(missing.c_str()));
                api.EOS_Shutdown();
                return finish();
            }
            EOS_HPlatform platform = CreatePlatform(api, creds);
            if (!platform)
            {
                step(L"platform", L"FAIL - EOS_Platform_Create returned null (see eos-sdk lines above)");
                api.EOS_Shutdown();
                return finish();
            }
            step(L"platform", L"ok");

            auto shutdown = [&]
            {
                api.EOS_Platform_Release(platform);
                api.EOS_Shutdown();
                finish();
            };
            EOS_HConnect connect = api.EOS_Platform_GetConnectInterface(platform);

            // 3. Device ID (an anonymous per-PC identity; "already exists" on later runs is fine)
            EOS_Connect_CreateDeviceIdOptions deviceOpts{};
            deviceOpts.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
            deviceOpts.DeviceModel = "PC";
            Completion<EOS_Connect_CreateDeviceIdCallbackInfo> device;
            api.EOS_Connect_CreateDeviceId(connect, &deviceOpts, &device, decltype(device)::Callback);
            if (!TickUntil(api, platform, device, 15000))
            {
                step(L"deviceid", L"FAIL - no response in 15 s (blocked network?)");
                return shutdown();
            }
            if (device.info.ResultCode != EOS_EResult::EOS_Success && device.info.ResultCode != EOS_EResult::EOS_DuplicateNotAllowed)
            {
                step(L"deviceid", L"FAIL - " + Result(api, device.info.ResultCode));
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
            api.EOS_Connect_Login(connect, &loginOpts, &login, decltype(login)::Callback);
            if (!TickUntil(api, platform, login, 15000))
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
                api.EOS_Connect_CreateUser(connect, &createOpts, &created, decltype(created)::Callback);
                if (!TickUntil(api, platform, created, 15000) || created.info.ResultCode != EOS_EResult::EOS_Success)
                {
                    step(L"login", L"FAIL - CreateUser: " + (created.done ? Result(api, created.info.ResultCode) : std::wstring(L"no response")));
                    return shutdown();
                }
                user = created.info.LocalUserId;
                step(L"login", L"ok (new product user " + UserId(api, user) + L")");
            }
            else if (login.info.ResultCode != EOS_EResult::EOS_Success)
            {
                step(L"login", L"FAIL - " + Result(api, login.info.ResultCode) + L" (check the client policy allows Connect logins)");
                return shutdown();
            }
            else
            {
                step(L"login", L"ok (product user " + UserId(api, user) + L")");
            }

            // 5. NAT type as EOS sees it (a query only)
            EOS_P2P_QueryNATTypeOptions natOpts{};
            natOpts.ApiVersion = EOS_P2P_QUERYNATTYPE_API_LATEST;
            Completion<EOS_P2P_OnQueryNATTypeCompleteInfo> nat;
            api.EOS_P2P_QueryNATType(api.EOS_Platform_GetP2PInterface(platform), &natOpts, &nat, decltype(nat)::Callback);
            if (!TickUntil(api, platform, nat, 15000))
                step(L"nat", L"FAIL - no response in 15 s");
            else if (nat.info.ResultCode != EOS_EResult::EOS_Success)
                step(L"nat", L"FAIL - " + Result(api, nat.info.ResultCode) + L" (check the client policy allows P2P)");
            else
                step(L"nat", std::wstring(L"ok (") + NatName(nat.info.NATType) + L")");

            // 6. Create a lobby with presence off (no Epic account features), then destroy it.
            EOS_HLobby lobbies = api.EOS_Platform_GetLobbyInterface(platform);
            EOS_Lobby_CreateLobbyOptions lobbyOpts{};
            lobbyOpts.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
            lobbyOpts.LocalUserId = user;
            lobbyOpts.MaxLobbyMembers = 4;
            lobbyOpts.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
            lobbyOpts.bPresenceEnabled = EOS_FALSE;
            lobbyOpts.bAllowInvites = EOS_FALSE;
            lobbyOpts.BucketId = "pdnet-eos-test";
            Completion<EOS_Lobby_CreateLobbyCallbackInfo> lobby;
            api.EOS_Lobby_CreateLobby(lobbies, &lobbyOpts, &lobby, decltype(lobby)::Callback);
            if (!TickUntil(api, platform, lobby, 15000))
            {
                step(L"lobby", L"FAIL - no response in 15 s");
            }
            else if (lobby.info.ResultCode != EOS_EResult::EOS_Success)
            {
                step(L"lobby", L"FAIL - " + Result(api, lobby.info.ResultCode) + L" (check the client policy allows Lobbies)");
            }
            else
            {
                EOS_Lobby_DestroyLobbyOptions destroyOpts{};
                destroyOpts.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
                destroyOpts.LocalUserId = user;
                destroyOpts.LobbyId = lobby.lobbyId.c_str();
                Completion<EOS_Lobby_DestroyLobbyCallbackInfo> destroyed;
                api.EOS_Lobby_DestroyLobby(lobbies, &destroyOpts, &destroyed, decltype(destroyed)::Callback);
                TickUntil(api, platform, destroyed, 10000);
                step(L"lobby", L"ok (created " + Wide(lobby.lobbyId.c_str()) + L", destroy: " +
                    (destroyed.done ? Result(api, destroyed.info.ResultCode) : std::wstring(L"no response")) + L")");
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
