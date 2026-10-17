#include "pch.h"
#include "hooks.h"
#include "gamesave.h"
#include "sda.h"
#include "log.h"
#include <set>

namespace pdnet
{
    namespace
    {
        using RoGetActivationFactoryFn = HRESULT(WINAPI*)(HSTRING, REFIID, void**);

        RoGetActivationFactoryFn RealRoGetActivationFactory()
        {
            static auto fn = reinterpret_cast<RoGetActivationFactoryFn>(GetProcAddress(GetModuleHandleW(L"combase.dll"), "RoGetActivationFactory"));
            return fn;
        }

        // Classes we answer ourselves instead of the OS.
        HRESULT TryOverride(std::wstring_view cls, REFIID iid, void** factory, bool& handled)
        {
            handled = false;
            if (cls == L"Windows.Gaming.XboxLive.Storage.GameSaveProvider")
            {
                static auto saves = CreateGameSaveProviderFactory();
                handled = true;
                return static_cast<IUnknown*>(winrt::get_abi(saves))->QueryInterface(iid, factory);
            }
            if (auto networking = GetXboxLiveNetworkingFactory(cls))
            {
                handled = true;
                return static_cast<IUnknown*>(winrt::get_abi(networking))->QueryInterface(iid, factory);
            }
            return E_NOTIMPL;
        }

        HRESULT WINAPI HookedRoGetActivationFactory(HSTRING classId, REFIID iid, void** factory)
        {
            UINT32 len = 0;
            const wchar_t* raw = WindowsGetStringRawBuffer(classId, &len);
            std::wstring_view cls(raw ? raw : L"", len);

            static std::mutex lock;
            static std::set<std::wstring, std::less<>> seen;
            bool first;
            {
                std::lock_guard guard(lock);
                first = seen.emplace(cls).second;
            }

            try
            {
                bool handled;
                HRESULT hr = TryOverride(cls, iid, factory, handled);
                if (handled)
                {
                    Log(L"activate %.*s -> pdnet (0x%08X)", static_cast<int>(cls.size()), cls.data(), hr);
                    return hr;
                }
            }
            catch (...)
            {
                HRESULT hr = winrt::to_hresult();
                Log(L"activate %.*s -> pdnet threw 0x%08X", static_cast<int>(cls.size()), cls.data(), hr);
                return hr;
            }

            HRESULT hr = RealRoGetActivationFactory()(classId, iid, factory);
            if (first || FAILED(hr))
            {
                Log(L"activate %.*s -> OS (0x%08X)", static_cast<int>(cls.size()), cls.data(), hr);
            }
            return hr;
        }

        bool IsWinRtImportDll(const char* name)
        {
            return _strnicmp(name, "api-ms-win-core-winrt-l1-1-", 27) == 0 || _stricmp(name, "combase.dll") == 0;
        }

        // Returns true if the module imports RoGetActivationFactory (patched now or earlier).
        bool PatchModule(HMODULE module)
        {
            auto base = reinterpret_cast<uint8_t*>(module);
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            {
                return false;
            }
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!dir.VirtualAddress)
            {
                return false;
            }
            bool found = false;
            for (auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); desc->Name; ++desc)
            {
                if (!IsWinRtImportDll(reinterpret_cast<const char*>(base + desc->Name)))
                {
                    continue;
                }
                auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
                auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
                for (; names->u1.AddressOfData; ++names, ++slots)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                    {
                        continue;
                    }
                    auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                    if (strcmp(reinterpret_cast<const char*>(byName->Name), "RoGetActivationFactory") != 0)
                    {
                        continue;
                    }
                    found = true;
                    auto hook = reinterpret_cast<ULONG_PTR>(&HookedRoGetActivationFactory);
                    if (slots->u1.Function != hook)
                    {
                        DWORD old;
                        VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &old);
                        slots->u1.Function = hook;
                        VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), old, &old);
                    }
                }
            }
            return found;
        }
    }

    void InstallActivationHooks(HMODULE self)
    {
        static std::mutex lock;
        static std::set<HMODULE> patched;
        std::lock_guard guard(lock);

        HMODULE modules[1024];
        DWORD needed = 0;
        if (!K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
        {
            return;
        }
        for (DWORD i = 0; i < (std::min)(static_cast<DWORD>(needed / sizeof(HMODULE)), static_cast<DWORD>(std::size(modules))); ++i)
        {
            HMODULE m = modules[i];
            if (m == self || patched.count(m))
            {
                continue;
            }
            if (PatchModule(m))
            {
                patched.insert(m);
                wchar_t name[MAX_PATH];
                GetModuleFileNameW(m, name, MAX_PATH);
                Log(L"hooked RoGetActivationFactory in %s", wcsrchr(name, L'\\') ? wcsrchr(name, L'\\') + 1 : name);
            }
        }
    }
}
