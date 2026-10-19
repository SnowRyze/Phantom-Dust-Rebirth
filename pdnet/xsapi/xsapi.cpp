// Glue between pdnet and its XSAPI stand-in (pdnet.ini: xbox=off).
#include "xsapi_pch.h"
#include "xsapi.h"

int32_t __stdcall WINRT_GetActivationFactory(void* classId, void** factory) noexcept;

namespace pdnet::xsapi
{
    void ReportStub(const char* function)
    {
        static std::mutex lock;
        static std::map<std::string, int> calls;
        int n;
        {
            std::lock_guard guard(lock);
            n = ++calls[function];
        }
        if (n <= 3) Log(L"xsapi-stub: %S called (not implemented yet)%s", function, n == 3 ? L" - further calls not logged" : L"");
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
