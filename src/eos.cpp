#include "pch.h"
#include "eos.h"
#include "config.h"
#include "log.h"

#ifdef PDNET_NO_EOS

namespace pdnet::eos
{
    void Start() { Log(L"eos: this pdnet build was compiled without the EOS SDK"); }
    bool SignedIn() { return false; }
    std::wstring LocalUserId() { return {}; }
    std::wstring DisplayName() { return GetConfig().displayName; }
    void Post(std::function<void(void*)>) {}
    void SetSignInListener(std::function<void(bool)>) {}
    void SetPump(std::function<void(void*)>) {}
    void Wake() {}
}

#else

#include "eossdk.h"
#include <deque>
#include <thread>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

namespace pdnet::eos
{
    namespace
    {
        using namespace eossdk;

        std::mutex g_lock;
        std::deque<std::function<void(void*)>> g_work;
        std::atomic<bool> g_signedIn{ false };
        std::atomic<bool> g_needSignIn{ true };
        std::wstring g_userId;
        std::function<void(bool)> g_signInListener; // guarded by g_lock
        std::function<void(void*)> g_pump;           // guarded by g_lock
        HANDLE g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        bool g_lostSignIn = false;                  // EOS thread only

        void NotifySignIn(bool signedIn)
        {
            std::function<void(bool)> listener;
            {
                std::lock_guard guard(g_lock);
                listener = g_signInListener;
            }
            if (listener) listener(signedIn);
        }

        void SetUser(const std::wstring& id)
        {
            std::lock_guard guard(g_lock);
            g_userId = id;
        }

        void EOS_CALL OnAuthExpiration(const EOS_Connect_AuthExpirationCallbackInfo*)
        {
            Log(L"eos: session about to expire, signing in again");
            g_needSignIn = true;
        }

        void EOS_CALL OnLoginStatusChanged(const EOS_Connect_LoginStatusChangedCallbackInfo* info)
        {
            Log(L"eos: login status %d -> %d", static_cast<int>(info->PreviousStatus), static_cast<int>(info->CurrentStatus));
            if (info->CurrentStatus == EOS_ELoginStatus::EOS_LS_NotLoggedIn)
            {
                g_signedIn = false;
                g_needSignIn = true;
                g_lostSignIn = true;
                NotifySignIn(false);
            }
        }

        // Like TickUntil, but keeps running the pump (P2P sends and receives): a sign-in again after the session expires
        // happens mid-match, and without the pump every player's game packets would wait until it finished.
        template <typename Info>
        bool TickPumping(const Api& api, EOS_HPlatform platform, const Completion<Info>& c, uint32_t timeoutMs)
        {
            auto end = GetTickCount64() + timeoutMs;
            while (!c.done && GetTickCount64() < end)
            {
                std::function<void(void*)> pump;
                {
                    std::lock_guard guard(g_lock);
                    pump = g_pump;
                }
                if (pump) pump(platform);
                api.EOS_Platform_Tick(platform);
                if (pump) pump(platform);
                WaitForSingleObject(g_wake, pump ? 2 : 20);
            }
            return c.done;
        }

        // Device-ID sign-in: create the device ID if needed, log in, create the product user on first use.
        bool SignIn(const Api& api, EOS_HPlatform platform)
        {
            EOS_HConnect connect = api.EOS_Platform_GetConnectInterface(platform);

            EOS_Connect_CreateDeviceIdOptions deviceOpts{};
            deviceOpts.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
            deviceOpts.DeviceModel = "PC";
            Completion<EOS_Connect_CreateDeviceIdCallbackInfo> device;
            api.EOS_Connect_CreateDeviceId(connect, &deviceOpts, &device, decltype(device)::Callback);
            if (!TickPumping(api, platform, device, 15000))
            {
                Log(L"eos: device ID: no response");
                return false;
            }
            if (device.info.ResultCode != EOS_EResult::EOS_Success && device.info.ResultCode != EOS_EResult::EOS_DuplicateNotAllowed)
            {
                Log(L"eos: device ID failed: %s", Result(api, device.info.ResultCode).c_str());
                return false;
            }

            std::string name = winrt::to_string(GetConfig().displayName);
            EOS_Connect_Credentials credentials{};
            credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
            credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
            EOS_Connect_UserLoginInfo loginInfo{};
            loginInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
            loginInfo.DisplayName = name.c_str();
            EOS_Connect_LoginOptions loginOpts{};
            loginOpts.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
            loginOpts.Credentials = &credentials;
            loginOpts.UserLoginInfo = &loginInfo;
            Completion<EOS_Connect_LoginCallbackInfo> login;
            api.EOS_Connect_Login(connect, &loginOpts, &login, decltype(login)::Callback);
            if (!TickPumping(api, platform, login, 15000))
            {
                Log(L"eos: login: no response");
                return false;
            }

            EOS_ProductUserId user = login.info.LocalUserId;
            if (login.info.ResultCode == EOS_EResult::EOS_InvalidUser && login.info.ContinuanceToken)
            {
                EOS_Connect_CreateUserOptions createOpts{};
                createOpts.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
                createOpts.ContinuanceToken = login.info.ContinuanceToken;
                Completion<EOS_Connect_CreateUserCallbackInfo> created;
                api.EOS_Connect_CreateUser(connect, &createOpts, &created, decltype(created)::Callback);
                if (!TickPumping(api, platform, created, 15000) || created.info.ResultCode != EOS_EResult::EOS_Success)
                {
                    Log(L"eos: creating the product user failed: %s", created.done ? Result(api, created.info.ResultCode).c_str() : L"no response");
                    return false;
                }
                user = created.info.LocalUserId;
            }
            else if (login.info.ResultCode != EOS_EResult::EOS_Success)
            {
                Log(L"eos: login failed: %s", Result(api, login.info.ResultCode).c_str());
                return false;
            }

            SetUser(UserId(api, user));
            g_signedIn = true;
            Log(L"eos: signed in as '%s' (product user %s)", GetConfig().displayName.c_str(), LocalUserId().c_str());
            return true;
        }

        void Run()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            std::wstring dir = ModuleDir();
            const Api* sdk = Load(dir);
            if (!sdk || !Initialize(*sdk, "pdnet-1")) return;
            const Api& api = *sdk;

            std::map<std::string, std::string> creds;
            if (auto missing = ReadCredentials(dir, creds); !missing.empty())
            {
                Log(L"eos: eos.ini next to the DLL is missing or has no %S - EOS disabled", missing.c_str());
                return;
            }
            EOS_HPlatform platform = CreatePlatform(api, creds);
            if (!platform)
            {
                Log(L"eos: EOS_Platform_Create failed - EOS disabled");
                return;
            }

            EOS_HConnect connect = api.EOS_Platform_GetConnectInterface(platform);
            EOS_Connect_AddNotifyAuthExpirationOptions expOpts{ EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST };
            api.EOS_Connect_AddNotifyAuthExpiration(connect, &expOpts, nullptr, OnAuthExpiration);
            EOS_Connect_AddNotifyLoginStatusChangedOptions statusOpts{ EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST };
            api.EOS_Connect_AddNotifyLoginStatusChanged(connect, &statusOpts, nullptr, OnLoginStatusChanged);

            uint32_t retryDelayMs = 5000;
            ULONGLONG nextAttempt = 0;
            for (;;)
            {
                if (g_needSignIn && GetTickCount64() >= nextAttempt)
                {
                    if (SignIn(api, platform))
                    {
                        g_needSignIn = false;
                        retryDelayMs = 5000;
                        if (g_lostSignIn)
                        {
                            g_lostSignIn = false;
                            NotifySignIn(true);
                        }
                    }
                    else
                    {
                        Log(L"eos: will retry sign-in in %u s", retryDelayMs / 1000);
                        nextAttempt = GetTickCount64() + retryDelayMs;
                        retryDelayMs = (std::min)(retryDelayMs * 3, 5u * 60 * 1000);
                    }
                }

                std::deque<std::function<void(void*)>> work;
                {
                    std::lock_guard guard(g_lock);
                    work.swap(g_work);
                }
                for (auto& item : work)
                {
                    try { item(platform); }
                    catch (...) { Log(L"eos: posted work threw 0x%08X", winrt::to_hresult()); }
                }

                std::function<void(void*)> pump;
                {
                    std::lock_guard guard(g_lock);
                    pump = g_pump;
                }
                if (pump) pump(platform);
                api.EOS_Platform_Tick(platform);
                if (pump) pump(platform);
                WaitForSingleObject(g_wake, pump ? 2 : 20);
            }
        }
    }

    void Start()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            Log(L"eos: starting (display name '%s')", GetConfig().displayName.c_str());
            std::thread([]
            {
                try { Run(); }
                catch (...) { Log(L"eos: thread crashed with 0x%08X", winrt::to_hresult()); }
            }).detach();
        });
    }

    bool SignedIn() { return g_signedIn; }

    std::wstring LocalUserId()
    {
        std::lock_guard guard(g_lock);
        return g_userId;
    }

    std::wstring DisplayName() { return GetConfig().displayName; }

    void Post(std::function<void(void*)> work)
    {
        std::lock_guard guard(g_lock);
        g_work.push_back(std::move(work));
    }

    void SetSignInListener(std::function<void(bool)> listener)
    {
        std::lock_guard guard(g_lock);
        g_signInListener = std::move(listener);
    }

    void SetPump(std::function<void(void*)> pump)
    {
        static std::once_flag timer;
        if (pump) std::call_once(timer, [] { timeBeginPeriod(1); }); // 2 ms waits really are 2 ms (default timer is ~15.6 ms)
        {
            std::lock_guard guard(g_lock);
            g_pump = std::move(pump);
        }
        SetEvent(g_wake);
    }

    void Wake() { SetEvent(g_wake); }
}

#endif
