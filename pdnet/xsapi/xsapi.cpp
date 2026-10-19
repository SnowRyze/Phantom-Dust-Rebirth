// Glue between pdnet and its XSAPI stand-in (pdnet.ini: xbox=off).
#include "xsapi_pch.h"
#include "xsapi.h"
#include <bcrypt.h>
#include <fstream>

#pragma comment(lib, "bcrypt.lib")

int32_t __stdcall WINRT_GetActivationFactory(void* classId, void** factory) noexcept;

namespace pdnet::xsapi
{
    void ReportStub(const char* function, bool returnedDefault)
    {
        static std::mutex lock;
        static std::map<std::string, int> calls;
        int n;
        {
            std::lock_guard guard(lock);
            n = ++calls[function];
        }
        if (n <= 3)
            Log(L"xsapi-stub: %S %s%s", function, returnedDefault ? L"returned a default (not implemented yet)" : L"NOT IMPLEMENTED (error returned)",
                n == 3 ? L" - further calls not logged" : L"");
    }

    const std::wstring& LocalXuid()
    {
        static std::wstring xuid = []
        {
            std::wstring dir = LocalStatePath() + L"\\pdnet";
            CreateDirectoryW(dir.c_str(), nullptr);
            std::wstring path = dir + L"\\identity.txt";
            std::wifstream in(path);
            std::wstring saved;
            if (in && std::getline(in, saved) && !saved.empty() && saved.find_first_not_of(L"0123456789") == std::wstring::npos)
                return saved;

            // 63-bit random id in the range real XUIDs use (2533274790395904 and up), so it looks like one.
            uint64_t r = 0;
            BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&r), sizeof(r), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            std::wstring id = std::to_wstring(2533274790395904ull + (r % 1'000'000'000'000ull));
            std::wofstream(path) << id;
            Log(L"xsapi-pdnet: new local player id %s", id.c_str());
            return id;
        }();
        return xuid;
    }

    HRESULT GetActivationFactory(HSTRING classId, IActivationFactory** factory)
    {
        *factory = nullptr;
        HRESULT hr = WINRT_GetActivationFactory(classId, reinterpret_cast<void**>(factory));
        if (SUCCEEDED(hr) && !*factory) hr = CLASS_E_CLASSNOTAVAILABLE;
        Log(L"xsapi-pdnet %s -> 0x%08X", WindowsGetStringRawBuffer(classId, nullptr), hr);
        return hr;
    }
}
