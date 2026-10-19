// pdnet: stands in for Microsoft.Xbox.Services.dll.
// Stage 1: forwards every XSAPI class to the original (renamed Microsoft.Xbox.Services.orig.dll),
// logs what the game activates, and replaces Xbox cloud saves with local files.
#include "pch.h"
#include "config.h"
#include "eostest.h"
#include "hooks.h"
#include "log.h"

namespace
{
    HMODULE g_self;
    using GetFactoryFn = HRESULT(WINAPI*)(HSTRING, IActivationFactory**);
    GetFactoryFn g_origGetFactory;

    void EnsureInitialized()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(g_self, path, MAX_PATH);
            pdnet::Log(L"pdnet loaded from %s (pid %lu)", path, GetCurrentProcessId());

            std::wstring orig = path;
            orig.replace(orig.find_last_of(L'\\') + 1, std::wstring::npos, L"Microsoft.Xbox.Services.orig.dll");
            HMODULE module = LoadLibraryExW(orig.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (module)
            {
                g_origGetFactory = reinterpret_cast<GetFactoryFn>(GetProcAddress(module, "DllGetActivationFactory"));
            }
            pdnet::Log(L"original XSAPI %s (error %lu)", g_origGetFactory ? L"loaded" : L"NOT loaded", module ? 0 : GetLastError());

            if (pdnet::GetConfig().eosTest)
            {
                pdnet::StartEosTest();
            }
        });
        pdnet::InstallActivationHooks(g_self);
    }
}

extern "C" HRESULT WINAPI PdGetActivationFactory(HSTRING classId, IActivationFactory** factory)
{
    EnsureInitialized();
    if (!g_origGetFactory)
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    HRESULT hr = g_origGetFactory(classId, factory);
    pdnet::Log(L"xsapi %s -> 0x%08X", WindowsGetStringRawBuffer(classId, nullptr), hr);
    return hr;
}

extern "C" HRESULT WINAPI PdCanUnloadNow()
{
    return S_FALSE;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
